//=============================================================================
// Copyright (c) 2015 LumakSoftware
//
//=============================================================================
#include <Urho3D/Urho3D.h>

#include <Urho3D/Core/Context.h>
#include <Urho3D/Core/Object.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/UIElement.h>
#include <Urho3D/UI/UIEvents.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/VertexBuffer.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Input/InputEvents.h>

#include <TurboBadger/tb_font_renderer.h>
#include <TurboBadger/tb_widgets.h>
#include <TurboBadger/tb_bitmap_fragment.h>
#include <TurboBadger/tb_system.h>
#include <TurboBadger/tb_msg.h>
#include <TurboBadger/tb_language.h>
#include <TurboBadger/animation/tb_animation.h>
#include <TurboBadger/animation/tb_widget_animation.h>

#include "TBWrapper.h"

#include <Urho3D/DebugNew.h>

//=============================================================================
//=============================================================================
#define QAL_VAL         0x60000000 // value to offset quality keys from all other keys in the same hash map

//=============================================================================
//=============================================================================
TUIRendererBatcher* TUIRendererBatcher::pSingleton_ = NULL;

//=============================================================================
//=============================================================================
UTBBitmap::UTBBitmap(Context *_pContext, int _width, int _height) 
    : context_( _pContext )
    , width_( _width ) 
    , height_( _height )
    , texture_( NULL )
{
    texture_ = new Texture2D( context_ );

    // set texture format
    texture_->SetMipsToSkip( QUALITY_LOW, 0 );
    texture_->SetNumLevels( 1 );
    texture_->SetSize( width_, height_, Graphics::GetRGBAFormat() );

    // set color and uv modes
    texture_->SetBorderColor( Color::TRANSPARENT );
    texture_->SetAddressMode( COORD_U, ADDRESS_WRAP );
    texture_->SetAddressMode( COORD_V, ADDRESS_WRAP );
}

//=============================================================================
//=============================================================================
UTBBitmap::~UTBBitmap()
{
    if ( texture_ )
    {
        texture_ = NULL;
    }
}

//=============================================================================
//=============================================================================
TUIRendererBatcher::TUIRendererBatcher(Context *_pContext, int _iwidth, int _iheight) 
    : UIElement( _pContext )
    , TBRendererBatcher() 
{
    SetPosition( 0, 0 );
    OnResizeWin( _iwidth, _iheight );
}

//=============================================================================
//=============================================================================
TUIRendererBatcher::~TUIRendererBatcher()
{
    vertexData_.Clear();
    batches_.Clear();
    uKeytoTBkeyMap.Clear();

    TBWidgetsAnimationManager::Shutdown();

    // shutdown
    tb_core_shutdown();
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::OnResizeWin(int _iwidth, int _iheight)
{
    m_screen_rect.x = 0;
    m_screen_rect.y = 0;
    m_screen_rect.w = _iwidth;
    m_screen_rect.h = _iheight;

    SetSize( _iwidth, _iheight );
    root_.SetRect( m_screen_rect );
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::Init(const String &_strDataPath)
{
    // register with UI
    GetSubsystem<UI>()->GetRoot()->AddChild( this );

    // store data path
    strDataPath_ = GetSubsystem<FileSystem>()->GetProgramDir() + _strDataPath;

    // init tb core
    tb_core_init( this );

    // load resources
    LoadDefaultResources();

    // map keys
    CreateKeyMap();

    // register handlers
    RegisterHandlers();
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::LoadDefaultResources()
{
    g_tb_lng->Load("resources/language/lng_en.tb.txt");

    // Load the default skin, and override skin that contains the graphics specific to the demo.
    g_tb_skin->Load("resources/default_skin/skin.tb.txt", "demo01/skin/skin.tb.txt");

    // **README**
    // - define TB_FONT_RENDERER_FREETYPE in tb_config.h for non-demo
#ifdef TB_FONT_RENDERER_TBBF
    void register_tbbf_font_renderer();
    register_tbbf_font_renderer();
#endif
#ifdef TB_FONT_RENDERER_STB
    void register_stb_font_renderer();
    register_stb_font_renderer();
#endif
#ifdef TB_FONT_RENDERER_FREETYPE
    void register_freetype_font_renderer();
    register_freetype_font_renderer();
#endif

    // Add fonts we can use to the font manager.
#if defined(TB_FONT_RENDERER_STB) || defined(TB_FONT_RENDERER_FREETYPE)
    g_font_manager->AddFontInfo("resources/vera.ttf", "Vera");
#endif
#ifdef TB_FONT_RENDERER_TBBF
    g_font_manager->AddFontInfo("resources/default_font/segoe_white_with_shadow.tb.txt", "Segoe");
    g_font_manager->AddFontInfo("fonts/neon.tb.txt", "Neon");
    g_font_manager->AddFontInfo("fonts/orangutang.tb.txt", "Orangutang");
    g_font_manager->AddFontInfo("fonts/orange.tb.txt", "Orange");
#endif

    // Set the default font description for widgets to one of the fonts we just added
    TBFontDescription fd;
#ifdef TB_FONT_RENDERER_TBBF
    fd.SetID(TBIDC("Segoe"));
#else
    fd.SetID(TBIDC("Vera"));
#endif
    fd.SetSize(g_tb_skin->GetDimensionConverter()->DpToPx(14));
    g_font_manager->SetDefaultFontDescription(fd);

    // Create the font now.
    TBFontFace *font = g_font_manager->CreateFontFace(g_font_manager->GetDefaultFontDescription());

    // Render some glyphs in one go now since we know we are going to use them. It would work fine
    // without this since glyphs are rendered when needed, but with some extra updating of the glyph bitmap.
    if ( font )
    {
        font->RenderGlyphs(" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~��������");
    }

    root_.SetSkinBg(TBIDC("background"));

    TBWidgetsAnimationManager::Init();
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::CreateKeyMap()
{
    // special keys
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_UP       , TB_KEY_UP        ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_DOWN     , TB_KEY_DOWN      ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_LEFT     , TB_KEY_LEFT      ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_RIGHT    , TB_KEY_RIGHT     ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_PAGEUP   , TB_KEY_PAGE_UP   ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_PAGEDOWN , TB_KEY_PAGE_DOWN ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_HOME     , TB_KEY_HOME      ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_END      , TB_KEY_END       ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_TAB      , TB_KEY_TAB       ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_BACKSPACE, TB_KEY_BACKSPACE ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_INSERT   , TB_KEY_INSERT    ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_DELETE   , TB_KEY_DELETE    ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_RETURN   , TB_KEY_ENTER     ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_ESC      , TB_KEY_ESC       ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F1       , TB_KEY_F1        ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F2       , TB_KEY_F2        ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F3       , TB_KEY_F3        ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F4       , TB_KEY_F4        ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F5       , TB_KEY_F5        ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F6       , TB_KEY_F6        ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F7       , TB_KEY_F7        ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F8       , TB_KEY_F8        ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F9       , TB_KEY_F9        ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F10      , TB_KEY_F10       ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F11      , TB_KEY_F11       ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( KEY_F12      , TB_KEY_F12       ) );

    // qualifiers: add QAL_VAL to qual keys to separate their range from rest of the keys
    uKeytoTBkeyMap.Insert( Pair<int,int>( QUAL_SHIFT + QAL_VAL, TB_SHIFT ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( QUAL_CTRL  + QAL_VAL, TB_CTRL  ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( QUAL_ALT   + QAL_VAL, TB_ALT   ) );
    uKeytoTBkeyMap.Insert( Pair<int,int>( QUAL_ANY   + QAL_VAL, TB_SUPER ) );
}

//=============================================================================
//=============================================================================
TBBitmap* TUIRendererBatcher::CreateBitmap(int width, int height, uint32 *data)
{
    UTBBitmap *pUTBBitmap = new UTBBitmap( GetContext(), width, height );

    FlushBitmap( (TBBitmap*)pUTBBitmap );

    pUTBBitmap->SetData( data );

    return (TBBitmap*)pUTBBitmap;
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::RenderBatch(Batch *_pb)
{
    if ( _pb )
    {
        UTBBitmap *pUTBBitmap = (UTBBitmap*)_pb->bitmap;
        SharedPtr<Texture2D> tdummy;
        SharedPtr<Texture2D> texture = pUTBBitmap?pUTBBitmap->texture_: tdummy;
        IntRect scissor( _pb->clipRect.x, _pb->clipRect.y, _pb->clipRect.x + _pb->clipRect.w, _pb->clipRect.y + _pb->clipRect.h );
        UIBatch batch( this, BLEND_ALPHA, scissor, texture, &vertexData_ );

        unsigned begin = batch.vertexData_->Size();
        batch.vertexData_->Resize(begin + _pb->vertex_count * UI_VERTEX_SIZE);
        float* dest = &(batch.vertexData_->At(begin));

        // set start/end
        batch.vertexStart_ = begin;
        batch.vertexEnd_   = batch.vertexData_->Size();

        for ( int i = 0; i < _pb->vertex_count; ++i )
        {
            dest[0+i*UI_VERTEX_SIZE]              = _pb->vertex[i].x; 
            dest[1+i*UI_VERTEX_SIZE]              = _pb->vertex[i].y; 
            dest[2+i*UI_VERTEX_SIZE]              = 0.0f;
            ((unsigned&)dest[3+i*UI_VERTEX_SIZE]) = _pb->vertex[i].col;
            dest[4+i*UI_VERTEX_SIZE]              = _pb->vertex[i].u; 
            dest[5+i*UI_VERTEX_SIZE]              = _pb->vertex[i].v;
        }

        // store
        UIBatch::AddOrMerge( batch, batches_ );
    }
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::GetBatches(PODVector<UIBatch>& batches, PODVector<float>& vertexData, const IntRect& currentScissor)
{
    for ( unsigned i = 0; i < batches_.Size(); ++i )
    {
        // get batch
        UIBatch &batch     = batches_[ i ];
        unsigned beg       = batch.vertexStart_;
        unsigned end       = batch.vertexEnd_;
        batch.vertexStart_ = vertexData.Size();
        batch.vertexEnd_   = vertexData.Size() + (end - beg);

        // resize and copy
        vertexData.Resize( batch.vertexEnd_ );
        memcpy( &vertexData[ batch.vertexStart_ ], &vertexData_[ beg ], (end - beg) * sizeof(float) );

        // store
        UIBatch::AddOrMerge( batch, batches );
    }
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::BeginPaint(int render_target_w, int render_target_h)
{
    TBRendererBatcher::BeginPaint( render_target_w, render_target_h );
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::EndPaint()
{
    TBRendererBatcher::EndPaint();

    // clear buffers
    vertexData_.Clear();
    batches_.Clear();
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::AddQuadInternal(const TBRect &dst_rect, const TBRect &src_rect, uint32 color, 
                                         TBBitmap *bitmap, TBBitmapFragment *fragment)
{
    if (batch.bitmap != bitmap)
    {
        batch.Flush(this);
        batch.bitmap = bitmap;
    }
    batch.fragment = fragment;

    // save clip rect
    batch.clipRect = m_clip_rect;

    if (bitmap)
    {
        int bitmap_w = bitmap->Width();
        int bitmap_h = bitmap->Height();
        m_u = (float)src_rect.x / bitmap_w;
        m_v = (float)src_rect.y / bitmap_h;
        m_uu = (float)(src_rect.x + src_rect.w) / bitmap_w;
        m_vv = (float)(src_rect.y + src_rect.h) / bitmap_h;
    }

    // change triangle winding order to clock-wise
    Vertex *ver = batch.Reserve(this, 6);
    ver[0].x = (float) dst_rect.x;
    ver[0].y = (float) (dst_rect.y + dst_rect.h);
    ver[0].u = m_u;
    ver[0].v = m_vv;
    ver[0].col = color;
    ver[2].x = (float) (dst_rect.x + dst_rect.w);
    ver[2].y = (float) (dst_rect.y + dst_rect.h);
    ver[2].u = m_uu;
    ver[2].v = m_vv;
    ver[2].col = color;
    ver[1].x = (float) dst_rect.x;
    ver[1].y = (float) dst_rect.y;
    ver[1].u = m_u;
    ver[1].v = m_v;
    ver[1].col = color;

    ver[3].x = (float) dst_rect.x;
    ver[3].y = (float) dst_rect.y;
    ver[3].u = m_u;
    ver[3].v = m_v;
    ver[3].col = color;
    ver[5].x = (float) (dst_rect.x + dst_rect.w);
    ver[5].y = (float) (dst_rect.y + dst_rect.h);
    ver[5].u = m_uu;
    ver[5].v = m_vv;
    ver[5].col = color;
    ver[4].x = (float) (dst_rect.x + dst_rect.w);
    ver[4].y = (float) dst_rect.y;
    ver[4].u = m_uu;
    ver[4].v = m_v;
    ver[4].col = color;

    // Update fragments batch id (See FlushBitmapFragment)
    if (fragment)
        fragment->m_batch_id = batch.batch_id;
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::RegisterHandlers()
{
    // timer
    SubscribeToEvent(E_UPDATE, HANDLER(TUIRendererBatcher, HandleUpdate));

    // screen resize and renderer
    SubscribeToEvent(E_RESIZED, HANDLER(TUIRendererBatcher, HandleScreenMode));
    SubscribeToEvent(E_BEGINFRAME, HANDLER(TUIRendererBatcher, HandleBeginFrame));
    SubscribeToEvent(E_POSTUPDATE, HANDLER(TUIRendererBatcher, HandlePostUpdate));
    SubscribeToEvent(E_POSTRENDERUPDATE, HANDLER(TUIRendererBatcher, HandlePostRenderUpdate));

    // inputs
    SubscribeToEvent(E_MOUSEBUTTONDOWN, HANDLER(TUIRendererBatcher, HandleMouseButtonDown));
    SubscribeToEvent(E_MOUSEBUTTONUP, HANDLER(TUIRendererBatcher, HandleMouseButtonUp));
    SubscribeToEvent(E_MOUSEMOVE, HANDLER(TUIRendererBatcher, HandleMouseMove));
    SubscribeToEvent(E_MOUSEWHEEL, HANDLER(TUIRendererBatcher, HandleMouseWheel));
    SubscribeToEvent(E_KEYDOWN, HANDLER(TUIRendererBatcher, HandleKeyDown));
    SubscribeToEvent(E_KEYUP, HANDLER(TUIRendererBatcher, HandleKeyUp));
    SubscribeToEvent(E_TEXTINPUT, HANDLER(TUIRendererBatcher, HandleTextInput));
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    // msg timer
    double t = TBMessageHandler::GetNextMessageFireTime();
    if ( t != TB_NOT_SOON && t <= TBSystem::GetTimeMS() )
    {
        TBMessageHandler::ProcessMessages();
    }
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandleScreenMode(StringHash eventType, VariantMap& eventData)
{
    using namespace Resized;

    OnResizeWin( eventData[P_WIDTH].GetInt(), eventData[P_HEIGHT].GetInt() );
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    TBAnimationManager::Update();
    root_.InvokeProcessStates();
    root_.InvokeProcess();
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandlePostUpdate(StringHash eventType, VariantMap& eventData)
{
    BeginPaint( root_.GetRect().w, root_.GetRect().h );

    root_.InvokePaint( TBWidget::PaintProps() );

    // If animations are running, reinvalidate immediately
    if ( TBAnimationManager::HasAnimationsRunning() )
    {
        root_.Invalidate();
    }
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    EndPaint();
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandleMouseButtonDown(StringHash eventType, VariantMap& eventData)
{
    using namespace MouseButtonDown;

    int mouseButtons = eventData[P_BUTTONS].GetInt();
    int qualifiers = eventData[P_QUALIFIERS].GetInt();
    MODIFIER_KEYS modKey = (MODIFIER_KEYS)FindTBKey( qualifiers + QAL_VAL );

    // exit if not the left mb
    if ( mouseButtons != MOUSEB_LEFT )
    {
        return;
    }

    root_.InvokePointerDown( lastMousePos_.x_, lastMousePos_.y_, 1, modKey, false );
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandleMouseButtonUp(StringHash eventType, VariantMap& eventData)
{
    using namespace MouseButtonUp;

    int mouseButtons = eventData[P_BUTTONS].GetInt();
    int qualifiers = eventData[P_QUALIFIERS].GetInt();
    MODIFIER_KEYS modKey = (MODIFIER_KEYS)FindTBKey( qualifiers + QAL_VAL );

    root_.InvokePointerUp( lastMousePos_.x_, lastMousePos_.y_, modKey, false );
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandleMouseMove(StringHash eventType, VariantMap& eventData)
{
    using namespace MouseMove;

    int mouseButtons = eventData[P_BUTTONS].GetInt();
    int qualifiers = eventData[P_QUALIFIERS].GetInt();
    MODIFIER_KEYS modKey = (MODIFIER_KEYS)FindTBKey( qualifiers + QAL_VAL );

    lastMousePos_ = IntVector2( eventData[P_X].GetInt(), eventData[P_Y].GetInt() );

    root_.InvokePointerMove( lastMousePos_.x_, lastMousePos_.y_, modKey, false );
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandleMouseWheel(StringHash eventType, VariantMap& eventData)
{
    using namespace MouseWheel;

    int mouseButtons = eventData[P_BUTTONS].GetInt();
    int qualifiers = eventData[P_QUALIFIERS].GetInt();
    int delta = eventData[P_WHEEL].GetInt();
    MODIFIER_KEYS modKey = (MODIFIER_KEYS)FindTBKey( qualifiers + QAL_VAL );

    root_.InvokeWheel( lastMousePos_.x_, lastMousePos_.y_, 0, -delta, modKey );
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandleKeyDown(StringHash eventType, VariantMap& eventData)
{
    using namespace KeyDown;

    int mouseButtons = eventData[P_BUTTONS].GetInt();
    int qualifiers = eventData[P_QUALIFIERS].GetInt();
    int key = eventData[P_KEY].GetInt();
    MODIFIER_KEYS modKey = (MODIFIER_KEYS)FindTBKey( qualifiers + QAL_VAL );
    SPECIAL_KEY spKey = (SPECIAL_KEY)FindTBKey( key );

    // exit if not a special key
    if ( spKey == TB_KEY_UNDEFINED )
    {
        return;
    }

    root_.InvokeKey( key, spKey, modKey, true );
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandleKeyUp(StringHash eventType, VariantMap& eventData)
{
    using namespace KeyUp;

    int mouseButtons = eventData[P_BUTTONS].GetInt();
    int qualifiers = eventData[P_QUALIFIERS].GetInt();
    int key = eventData[P_KEY].GetInt();
    MODIFIER_KEYS modKey = (MODIFIER_KEYS)FindTBKey( qualifiers + QAL_VAL );
    SPECIAL_KEY spKey = (SPECIAL_KEY)FindTBKey( key );

    root_.InvokeKey( key, spKey, modKey, false );
}

//=============================================================================
//=============================================================================
void TUIRendererBatcher::HandleTextInput(StringHash eventType, VariantMap& eventData)
{
    using namespace TextInput;

    int mouseButtons = eventData[P_BUTTONS].GetInt();
    int qualifiers = eventData[P_QUALIFIERS].GetInt();
    String str = eventData[ P_TEXT ].GetString();
    int key = (int)str.CString()[ 0 ];
    MODIFIER_KEYS modKey = (MODIFIER_KEYS)FindTBKey( qualifiers + QAL_VAL );
    SPECIAL_KEY spKey = TB_KEY_UNDEFINED;

    root_.InvokeKey( key, spKey, modKey, true );
}

//=============================================================================
//=============================================================================
class UTBFile : public TBFile
{
public:
    UTBFile(Context *_pContext) 
        : ufileSize_( 0 )
    {
        pfile_ = new File( _pContext );
    }

    virtual ~UTBFile() 
    { 
        if ( pfile_ )
        {
            pfile_->Close();
            pfile_ = NULL;
        }
    }

    bool OpenFile(const char* fileName)
    {
        bool bopen = pfile_->Open( fileName );

        if ( bopen )
        {
            ufileSize_ = pfile_->Seek( (unsigned)-1 );
            pfile_->Seek( 0 );
        }

        return bopen;
    }

    virtual long Size()
    {
        return (long)ufileSize_;
    }

    virtual size_t Read(void *buf, size_t elemSize, size_t count)
    {
        return pfile_->Read( buf, elemSize * count );
    }

protected:
    SharedPtr<File> pfile_;
    unsigned ufileSize_;
};

//=============================================================================
// static
//=============================================================================
TBFile* TBFile::Open(const char *filename, TBFileMode )
{
    UTBFile *pFile = new UTBFile( TUIRendererBatcher::Singleton().GetContext() );
    String strFilename = TUIRendererBatcher::Singleton().GetDataPath() + String( filename );

    if ( !pFile->OpenFile( strFilename.CString() ) )
    {
        delete pFile;
        pFile = NULL;
    }

    return pFile;
}
