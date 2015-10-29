//=============================================================================
// Copyright (c) 2015 LumakSoftware
//
//=============================================================================
#pragma once

#include <Urho3D/Urho3D.h>
#include <TurboBadger/tb_widgets.h>
#include <TurboBadger/tb_renderer.h>
#include <TurboBadger/renderers/tb_renderer_batcher.h>

namespace Urho3D
{
class Context;
class VertexBuffer;
class Texture2D;
}

//=============================================================================
//=============================================================================
using namespace Urho3D;
using namespace tb;

//=============================================================================
//=============================================================================
class UTBBitmap : public TBBitmap
{
public:
    UTBBitmap(Context *_pContext, int _width, int _height); 
    ~UTBBitmap();

    // =========== virtual methods required for TBBitmap subclass =========
	virtual void SetData(uint32 *_pdata)
    {
        texture_->SetData( 0, 0, 0, width_, height_, _pdata );
    }

    virtual int Width() { return width_; }
    virtual int Height(){ return height_; }

    SharedPtr<Context>      context_;
    SharedPtr<Texture2D>    texture_;
    int                     width_;
    int                     height_;
};

//=============================================================================
//=============================================================================
class TUIRendererBatcher : public UIElement, public TBRendererBatcher
{
    OBJECT( TUIRendererBatcher )
public:
    // static funcs
    static void Create(Context *_pContext, int _iwidth, int _iheight)
    {
        if ( pSingleton_ == NULL )
        {
            pSingleton_ = new TUIRendererBatcher( _pContext, _iwidth, _iheight );
        }
    }

    static void Destroy()
    {
        if ( pSingleton_ )
        {
            pSingleton_->Remove();
            pSingleton_ = NULL;
        }
    }

    static TUIRendererBatcher& Singleton() { return *pSingleton_; }

    // methods
    void Init(const String &_strDataPath);
    void LoadDefaultResources();

    TBWidget& Root() { return root_; }
    const String& GetDataPath() { return strDataPath_; }

    // override funcs
    virtual void BeginPaint(int render_target_w, int render_target_h);
    virtual void EndPaint();

    // UIElement override method to add TB batches
    virtual void GetBatches(PODVector<UIBatch>& batches, PODVector<float>& vertexData, const IntRect& currentScissor);

	// ===== methods that need implementation in TBRendererBatcher subclasses =====
	virtual TBBitmap* CreateBitmap(int width, int height, uint32 *data);

    virtual void RenderBatch(Batch *batch);

	virtual void SetClipRect(const TBRect &rect)
    {
        m_clip_rect = rect;
    }

protected:
    // override methods
    virtual void AddQuadInternal(const TBRect &dst_rect, const TBRect &src_rect, uint32 color, TBBitmap *bitmap, TBBitmapFragment *fragment);

protected:
    TUIRendererBatcher(Context *_pContext, int _iwidth, int _iheight);
    virtual ~TUIRendererBatcher();

    void OnResizeWin(int _iwidth, int _iheight);
    void CreateKeyMap();
    void RegisterHandlers();

    void HandleUpdate(StringHash eventType, VariantMap& eventData);

    // renderer
    void HandleScreenMode(StringHash eventType, VariantMap& eventData);
    void HandleBeginFrame(StringHash eventType, VariantMap& eventData);
    void HandlePostUpdate(StringHash eventType, VariantMap& eventData);
    void HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData);

    // inputs
    void HandleMouseButtonDown(StringHash eventType, VariantMap& eventData);
    void HandleMouseButtonUp(StringHash eventType, VariantMap& eventData);
    void HandleMouseMove(StringHash eventType, VariantMap& eventData);
    void HandleMouseWheel(StringHash eventType, VariantMap& eventData);
    void HandleKeyDown(StringHash eventType, VariantMap& eventData);
    void HandleKeyUp(StringHash eventType, VariantMap& eventData);
    void HandleTextInput(StringHash eventType, VariantMap& eventData);

    // TB special and quality keys func
    int FindTBKey(int _ikey)
    {
        HashMap<int, int>::Iterator itr = uKeytoTBkeyMap.Find( _ikey );
        int itbkey = 0;

        if ( itr != uKeytoTBkeyMap.End() )
        {
            itbkey = itr->second_;
        }
        return itbkey;
    }

protected:
    static TUIRendererBatcher   *pSingleton_;

    TBWidget            root_;
    PODVector<float>    vertexData_;
    PODVector<UIBatch>  batches_;

    String              strDataPath_;

    HashMap<int, int>   uKeytoTBkeyMap;
    IntVector2          lastMousePos_;
};

