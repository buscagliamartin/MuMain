#pragma once

#include "UI/NewUI/NewUIBase.h"
#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUI3DRenderMng.h"
#include "UI/NewUI/Widgets/NewUIButton.h"
#include "UI/Legacy/UIControls.h"

namespace SEASON3B
{
    // BarnaMu: account-wide Auction House window, replacing the personal store player path.
    class CNewUIAuctionHouse : public CNewUIObj, public INewUI3DRenderObj
    {
    public:
        struct ListingView
        {
            unsigned int ListingNumber;
            unsigned int Price;
            unsigned short ItemType;
            BYTE ItemLevel;
            BYTE Currency;
            BYTE JewelSlot;
            BYTE Status;
            wchar_t ItemName[48];
            wchar_t SellerName[12];
            // BarnaMu Phase 1: the serialized item payload the 0xBF/0x31 packet already carries, so a
            // listing can show the real item level + options instead of the unreliable ItemLevel byte.
            // The optional payload is EITHER raw item bytes (ItemData/ItemDataLength) OR a text
            // summary (ItemSummary), depending on what the server sent for that listing.
            BYTE ItemDataLength;
            BYTE ItemData[15];
            wchar_t ItemSummary[256];
            int RealLevel;          // decoded item level from ItemData; -1 when unknown (show no suffix)
            wchar_t Options[64];    // compact decoded options (e.g. "Excellent Luck"); empty when none
        };

        CNewUIAuctionHouse();
        ~CNewUIAuctionHouse();

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

        void Toggle();
        void SetListingsHeader(BYTE view, BYTE page, BYTE count);
        void AddListing(const ListingView& listing);
        void SetStatusMessage(const wchar_t* message);
        bool IsCreateListingView() const;
        bool TrySetCreateListingItemFromInventorySlot(int slot);

    public:
        enum IMAGE_LIST
        {
            IMAGE_BASE_WINDOW_BTN_EXIT = BITMAP_INTERFACE_NEW_PERSONALINVENTORY_BEGIN + 17,
            IMAGE_TABLE_TOP_LEFT = BITMAP_EFFECT_TEXTURE_BEGIN + 55,
            IMAGE_TABLE_TOP_RIGHT,
            IMAGE_TABLE_BOTTOM_LEFT,
            IMAGE_TABLE_BOTTOM_RIGHT,
            IMAGE_TABLE_TOP_PIXEL,
            IMAGE_TABLE_BOTTOM_PIXEL,
            IMAGE_TABLE_LEFT_PIXEL,
            IMAGE_TABLE_RIGHT_PIXEL,
            IMAGE_IGS_BUTTON = BITMAP_IGS_MSGBOX_BUTTON,
            IMAGE_ROUND_BUTTON = BITMAP_CATAPULT_BEGIN + 2,
        };

    private:
        static constexpr int WINDOW_WIDTH = 430;
        static constexpr int WINDOW_HEIGHT = 286;
        static constexpr int MAX_ROWS = 10;

        void SetPos(int x, int y);
        void LoadImages();
        void UnloadImages();
        void InitButtons();
        void SendRequest(BYTE op, BYTE arg1, BYTE currency, BYTE jewelSlot, unsigned int arg2, unsigned int arg3);
        void RequestCurrentView();
        void SetView(BYTE view);
        void SetFilter(BYTE filter);
        void ClearCreateSelection();
        void AddSelectedInventoryItemToCreateListing();
        void SetStatusMessages(const wchar_t* line1, const wchar_t* line2);
        bool ProcessMouseButtons();
        bool SendSelectedListingAction(BYTE op, const wchar_t* action);
        bool SendCreateListingAction();
        bool TryGetSellPrice(unsigned int& price) const;
        int GetSelectedInventorySlot() const;
        void RenderBack();
        void RenderTable();
        void RenderFilters();
        void RenderDetails();
        void RenderCreateListing();
        void RenderFooter();
        void RenderContextAction();
        void RenderListingActionButton(CNewUIButton& button, const wchar_t* text, bool enabled, int tone);
        const wchar_t* GetCurrencyText(BYTE currency, BYTE jewelSlot) const;
        const wchar_t* GetStatusText(BYTE status) const;
        const wchar_t* GetViewTitle() const;
        const wchar_t* GetActionText() const;

    private:
        CNewUIManager* m_pNewUIMng;
        CNewUI3DRenderMng* m_pNewUI3DRenderMng;
        POINT m_Pos;
        BYTE m_CurrentView;
        BYTE m_CurrentPage;
        BYTE m_Filter;
        BYTE m_SellCurrency;
        BYTE m_SellJewelSlot;
        int m_CreateSlot;
        short m_CreateItemType;
        int m_CreateItemLevel;
        int m_SelectedRow;
        int m_HoveredRow;
        int m_RowCount;
        ListingView m_Listings[MAX_ROWS];
        wchar_t m_StatusMessage[128];
        wchar_t m_StatusMessage2[128];
        CUITextInputBox m_PriceInput;
        CNewUIButton m_BtnHelp;
        CNewUIButton m_BtnMinimize;
        CNewUIButton m_BtnBrowse;
        CNewUIButton m_BtnMine;
        CNewUIButton m_BtnDeliveries;
        CNewUIButton m_BtnPayouts;
        CNewUIButton m_BtnCreate;
        CNewUIButton m_BtnMyBidsDisabled;
        CNewUIButton m_BtnWatchlistDisabled;
        CNewUIButton m_BtnHistoryDisabled;
        CNewUIButton m_BtnRefresh;
        CNewUIButton m_BtnPrev;
        CNewUIButton m_BtnNext;
        CNewUIButton m_BtnFilterAll;
        CNewUIButton m_BtnFilterZen;
        CNewUIButton m_BtnFilterWCoin;
        CNewUIButton m_BtnFilterJewel;
        CNewUIButton m_BtnFilterApply;
        CNewUIButton m_BtnFilterReset;
        CNewUIButton m_BtnAdvancedFiltersDisabled;
        CNewUIButton m_BtnSellZen;
        CNewUIButton m_BtnSellWCoin;
        CNewUIButton m_BtnSellJewel;
        CNewUIButton m_BtnSellJewelType;
        CNewUIButton m_BtnAddItem;
        CNewUIButton m_BtnClearItem;
        CNewUIButton m_BtnPostListing;
        CNewUIButton m_BtnBuy;
        CNewUIButton m_BtnCancelListing;
        CNewUIButton m_BtnReceiveItem;
        CNewUIButton m_BtnClaimPayout;
        CNewUIButton m_BtnPlaceBidDisabled;
        CNewUIButton m_BtnCompareDisabled;
        CNewUIButton m_BtnReportDisabled;
        CNewUIButton m_BtnClose;
    };
}
