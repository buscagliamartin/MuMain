#pragma once

#include "UI/NewUI/NewUIBase.h"
#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUI3DRenderMng.h"
#include "UI/NewUI/Widgets/NewUIButton.h"

namespace SEASON3B
{
    // BarnaMu: per-account Jewel Bank window.
    //
    // Extracted verbatim (Client Feature Bundle Step 3) from the reference client's
    // NewUIMuHelper.cpp/.h, where it was an independent class that merely lived in those files.
    // It is a standalone window here so the clean MuHelper mode foundation stays untouched.
    // Deposit / withdraw a single jewel or a 10-pack per row, plus a deposit-everything button and
    // a Deposit<->Withdraw mode toggle; balances arrive via the already-merged server Jewel Bank
    // packet (0xBF / 0x30). In the reference it was opened from the MU Helper menu; in clean it is
    // opened with the J hotkey (manager-driven, like the other ported windows).
    class CNewUIJewelBank : public CNewUIObj, public INewUI3DRenderObj
    {
    public:
        CNewUIJewelBank();
        ~CNewUIJewelBank();

        bool Create(CNewUIManager* pNewUIMng, CNewUI3DRenderMng* pNewUI3DRenderMng, int x, int y);
        void Release();

        bool Render();
        void Render3D();
        bool Update();
        bool UpdateMouseEvent();
        bool UpdateKeyEvent();
        bool IsVisible() const override;

        float GetLayerDepth();
        float GetKeyEventOrder();

    public:
        void Toggle();
        void SetBalances(const unsigned int* pBalances);

        static constexpr int ITEM_COUNT = 17;

    public:
        enum IMAGE_LIST
        {
            IMAGE_BASE_WINDOW_BACK = BITMAP_INTERFACE_NEW_MESSAGEBOX_BEGIN + 3,
            IMAGE_BASE_WINDOW_TOP = BITMAP_INTERFACE_NEW_PERSONALINVENTORY_BEGIN,
            IMAGE_BASE_WINDOW_LEFT = BITMAP_INTERFACE_NEW_PERSONALINVENTORY_BEGIN + 2,
            IMAGE_BASE_WINDOW_RIGHT = BITMAP_INTERFACE_NEW_PERSONALINVENTORY_BEGIN + 3,
            IMAGE_BASE_WINDOW_BOTTOM = BITMAP_INTERFACE_NEW_PERSONALINVENTORY_BEGIN + 4,
            IMAGE_BASE_WINDOW_BTN_EXIT = BITMAP_INTERFACE_NEW_PERSONALINVENTORY_BEGIN + 17,
            IMAGE_ITEM_BOX = BITMAP_INTERFACE_NEW_INVENTORY_BASE_BEGIN,
            IMAGE_TABLE_TOP_LEFT,
            IMAGE_TABLE_TOP_RIGHT,
            IMAGE_TABLE_BOTTOM_LEFT,
            IMAGE_TABLE_BOTTOM_RIGHT,
            IMAGE_TABLE_TOP_PIXEL,
            IMAGE_TABLE_BOTTOM_PIXEL,
            IMAGE_TABLE_LEFT_PIXEL,
            IMAGE_TABLE_RIGHT_PIXEL,
            IMAGE_IGS_BUTTON = BITMAP_IGS_MSGBOX_BUTTON,
            IMAGE_ROUND_BUTTON = BITMAP_CATAPULT_BEGIN + 1,
        };

    private:
        static constexpr int WINDOW_WIDTH = 470;
        static constexpr int WINDOW_HEIGHT = 396;
        static constexpr int ROW_START_Y = 64;
        static constexpr int ROW_HEIGHT = 17;

        void SetPos(int x, int y);
        void InitButtons();
        void LoadImages();
        void UnloadImages();
        void SendRequest(BYTE op, BYTE arg1, WORD arg2, WORD arg3);
        void RenderBack();
        void RenderTable();

    private:
        CNewUIManager* m_pNewUIMng;
        CNewUI3DRenderMng* m_pNewUI3DRenderMng;
        POINT m_Pos;
        bool m_DepositMode;
        unsigned int m_Balances[ITEM_COUNT];
        CNewUIButton m_BtnActionSingle[ITEM_COUNT];
        CNewUIButton m_BtnActionPack[ITEM_COUNT];
        CNewUIButton m_BtnModeToggle;
        CNewUIButton m_BtnDepositAll;
        CNewUIButton m_BtnClose;
    };
}
