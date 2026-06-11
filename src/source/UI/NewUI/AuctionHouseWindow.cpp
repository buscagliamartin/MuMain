#include "stdafx.h"

#include <cwchar>

#include "UI/NewUI/AuctionHouseWindow.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Inventory/NewUIItemMng.h"
#include "UI/NewUI/Inventory/NewUIMyInventory.h"
#include "Engine/Object/ZzzInventory.h"
#include "UI/Legacy/UIControls.h"
#include "Dotnet/Connection.h"

extern Connection* SocketClient;

using namespace SEASON3B;

static void RenderJewelBankRect(int x, int y, int width, int height, float red, float green, float blue, float alpha)
{
    if (width <= 0 || height <= 0 || alpha <= 0.f)
        return;

    glColor4f(red, green, blue, alpha);
    glDisable(GL_TEXTURE_2D);
    RenderColor(float(x), float(y), float(width), float(height));
    EndRenderColor();
    glColor4f(1.f, 1.f, 1.f, 1.f);
}

static void RenderJewelBankLine(int x, int y, int width, int height, float red, float green, float blue, float alpha)
{
    RenderJewelBankRect(x, y, width, height, red, green, blue, alpha);
}

// BarnaMu: CNewUIAuctionHouse - DB escrow auction house client window.
//////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr BYTE AUCTION_VIEW_BROWSE = 0;
    constexpr BYTE AUCTION_VIEW_MINE = 1;
    constexpr BYTE AUCTION_VIEW_DELIVERIES = 2;
    constexpr BYTE AUCTION_VIEW_PAYOUTS = 3;
    constexpr BYTE AUCTION_VIEW_CREATE = 4;

    constexpr int AUCTION_TAB_Y = 34;
    constexpr int AUCTION_FILTER_X = 8;
    constexpr int AUCTION_FILTER_Y = 58;
    constexpr int AUCTION_FILTER_WIDTH = 72;
    constexpr int AUCTION_FILTER_HEIGHT = 188;
    constexpr int AUCTION_TABLE_PANEL_X = 86;
    constexpr int AUCTION_TABLE_PANEL_Y = 58;
    constexpr int AUCTION_TABLE_PANEL_WIDTH = 208;
    constexpr int AUCTION_TABLE_PANEL_HEIGHT = 188;
    constexpr int AUCTION_TABLE_X = 92;
    constexpr int AUCTION_TABLE_Y = 94;
    constexpr int AUCTION_HEADER_HEIGHT = 15;
    constexpr int AUCTION_ROW_HEIGHT = 13;
    constexpr int AUCTION_TABLE_WIDTH = 196;
    constexpr int AUCTION_DETAILS_X = 300;
    constexpr int AUCTION_DETAILS_Y = 58;
    constexpr int AUCTION_DETAILS_WIDTH = 122;
    constexpr int AUCTION_DETAILS_HEIGHT = 188;
    constexpr int AUCTION_CREATE_X = 86;
    constexpr int AUCTION_CREATE_Y = 58;
    constexpr int AUCTION_CREATE_WIDTH = 336;
    constexpr int AUCTION_CREATE_HEIGHT = 188;
    constexpr int AUCTION_BOTTOM_X = 8;
    constexpr int AUCTION_BOTTOM_Y = 250;
    constexpr int AUCTION_BOTTOM_WIDTH = 414;
    constexpr int AUCTION_BOTTOM_HEIGHT = 28;
    constexpr int AUCTION_COLUMN_COUNT = 6;
    constexpr int s_AuctionColumns[AUCTION_COLUMN_COUNT] =
    {
        0, 76, 100, 140, 164, 196,
    };

    const wchar_t* const s_AuctionHeaders[AUCTION_COLUMN_COUNT - 1] =
    {
        L"Item",
        L"Lvl",
        L"Seller",
        L"Cur",
        L"Price",
    };

    const wchar_t* const s_AuctionJewelNames[17] =
    {
        L"Bless",
        L"Soul",
        L"Life",
        L"Creation",
        L"Guardian",
        L"Gemstone",
        L"Harmony",
        L"Chaos",
        L"Low Ref",
        L"High Ref",
        L"BoK +1",
        L"BoK +2",
        L"BoK +3",
        L"BoK +4",
        L"BoK +5",
        L"Blue Choco",
        L"Pink Choco",
    };

    void SetupAuctionButton(CNewUIButton& button, int x, int y, int width, int height, const wchar_t* text, const wchar_t* tooltip)
    {
        button.ChangeButtonImgState(1, CNewUIAuctionHouse::IMAGE_IGS_BUTTON, 1, 0, 1);
        button.ChangeButtonInfo(x, y, width, height);
        button.ChangeText(text);
        button.ChangeToolTipText(tooltip, TRUE);
    }

    enum AuctionButtonTone
    {
        AUCTION_TONE_NEUTRAL,
        AUCTION_TONE_CONFIRM,
        AUCTION_TONE_BUY,
        AUCTION_TONE_DISABLED,
    };

    void RenderAuctionPanel(int x, int y, int width, int height, const wchar_t* title)
    {
        RenderJewelBankRect(x, y, width, height, 0.015f, 0.017f, 0.020f, 0.92f);
        RenderJewelBankRect(x + 1, y + 1, width - 2, height - 2, 0.055f, 0.065f, 0.078f, 0.88f);
        RenderJewelBankRect(x + 3, y + 3, width - 6, 18, 0.095f, 0.100f, 0.110f, 0.92f);
        RenderJewelBankLine(x, y, width, 1, 0.46f, 0.39f, 0.24f, 0.70f);
        RenderJewelBankLine(x, y + height - 1, width, 1, 0.07f, 0.08f, 0.09f, 0.92f);
        RenderJewelBankLine(x, y, 1, height, 0.35f, 0.34f, 0.31f, 0.68f);
        RenderJewelBankLine(x + width - 1, y, 1, height, 0.06f, 0.07f, 0.08f, 0.90f);

        if (title != NULL)
        {
            g_pRenderText->SetFont(g_hFontBold);
            g_pRenderText->SetBgColor(0);
            g_pRenderText->SetTextColor(226, 190, 112, 255);
            g_pRenderText->RenderText(x + 6, y + 6, title, width - 12, 0, RT3_SORT_CENTER);
        }
    }

    void RenderAuctionButton(CNewUIButton& button, const wchar_t* text, bool enabled, int tone)
    {
        const POINT& pos = button.GetPos();
        const POINT& size = button.GetSize();
        const BUTTON_STATE state = button.GetBTState();
        const bool hot = enabled && state == BUTTON_STATE_OVER;
        const bool down = enabled && state == BUTTON_STATE_DOWN;

        float red = 0.12f, green = 0.13f, blue = 0.15f;
        if (tone == AUCTION_TONE_CONFIRM)
        {
            red = 0.05f; green = 0.23f; blue = 0.11f;
        }
        else if (tone == AUCTION_TONE_BUY)
        {
            red = 0.34f; green = 0.22f; blue = 0.04f;
        }
        else if (tone == AUCTION_TONE_DISABLED)
        {
            red = 0.08f; green = 0.08f; blue = 0.09f;
        }

        const float light = !enabled ? 0.58f : (down ? 0.78f : (hot ? 1.18f : 1.0f));
        RenderJewelBankRect(pos.x, pos.y, size.x, size.y, 0.015f, 0.016f, 0.018f, 0.94f);
        RenderJewelBankRect(pos.x + 1, pos.y + 1, size.x - 2, size.y - 2, red * light, green * light, blue * light, enabled ? 0.92f : 0.62f);
        RenderJewelBankLine(pos.x + 2, pos.y + 2, size.x - 4, 1, 0.58f * light, 0.50f * light, 0.34f * light, enabled ? 0.70f : 0.26f);
        RenderJewelBankLine(pos.x + 2, pos.y + size.y - 2, size.x - 4, 1, 0.02f, 0.02f, 0.025f, 0.82f);

        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetBgColor(0);
        if (!enabled)
            g_pRenderText->SetTextColor(120, 124, 128, 255);
        else if (tone == AUCTION_TONE_CONFIRM)
            g_pRenderText->SetTextColor(180, 245, 194, 255);
        else if (tone == AUCTION_TONE_BUY)
            g_pRenderText->SetTextColor(255, 211, 112, 255);
        else
            g_pRenderText->SetTextColor(218, 222, 224, 255);

        const int textY = pos.y + (size.y > 20 ? 8 : 5);
        g_pRenderText->RenderText(pos.x, textY, text, size.x, 0, RT3_SORT_CENTER);
    }

    void RenderAuctionLargeButton(CNewUIButton& button, const wchar_t* line1, const wchar_t* line2, bool enabled, int tone)
    {
        RenderAuctionButton(button, L"", enabled, tone);

        const POINT& pos = button.GetPos();
        const POINT& size = button.GetSize();
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetBgColor(0);
        if (!enabled)
            g_pRenderText->SetTextColor(128, 132, 136, 255);
        else if (tone == AUCTION_TONE_BUY)
            g_pRenderText->SetTextColor(255, 216, 122, 255);
        else
            g_pRenderText->SetTextColor(190, 246, 204, 255);

        if (line2 == NULL || line2[0] == L'\0')
        {
            g_pRenderText->RenderText(pos.x + 4, pos.y + 8, line1, size.x - 8, 0, RT3_SORT_CENTER);
        }
        else
        {
            g_pRenderText->RenderText(pos.x + 4, pos.y + 4, line1, size.x - 8, 0, RT3_SORT_CENTER);
            g_pRenderText->RenderText(pos.x + 4, pos.y + 14, line2, size.x - 8, 0, RT3_SORT_CENTER);
        }
    }

    void RenderAuctionTab(CNewUIButton& button, const wchar_t* text, bool selected, bool enabled)
    {
        const POINT& pos = button.GetPos();
        const POINT& size = button.GetSize();
        const BUTTON_STATE state = button.GetBTState();
        const bool hot = enabled && state == BUTTON_STATE_OVER;

        RenderJewelBankRect(pos.x, pos.y, size.x, size.y, 0.012f, 0.014f, 0.018f, 0.94f);
        if (!enabled)
            RenderJewelBankRect(pos.x + 1, pos.y + 1, size.x - 2, size.y - 2, 0.055f, 0.058f, 0.064f, 0.72f);
        else if (selected)
            RenderJewelBankRect(pos.x + 1, pos.y + 1, size.x - 2, size.y - 2, 0.05f, 0.17f, 0.30f, 0.92f);
        else
            RenderJewelBankRect(pos.x + 1, pos.y + 1, size.x - 2, size.y - 2, 0.070f, 0.080f, 0.095f, hot ? 0.94f : 0.80f);

        if (selected || hot)
            RenderJewelBankLine(pos.x + 2, pos.y + size.y - 2, size.x - 4, 1, 0.16f, 0.48f, 0.88f, 0.88f);

        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetBgColor(0);
        if (!enabled)
            g_pRenderText->SetTextColor(112, 116, 122, 255);
        else if (selected)
            g_pRenderText->SetTextColor(235, 207, 138, 255);
        else
            g_pRenderText->SetTextColor(214, 218, 222, 255);

        g_pRenderText->RenderText(pos.x, pos.y + 7, text, size.x, 0, RT3_SORT_CENTER);
    }

    void RenderAuctionSeparator(int x, int y, int width)
    {
        RenderJewelBankLine(x, y, width, 1, 0.42f, 0.35f, 0.22f, 0.42f);
        RenderJewelBankLine(x, y + 1, width, 1, 0.00f, 0.00f, 0.00f, 0.34f);
    }

    bool IsAuctionListingCancellable(const CNewUIAuctionHouse::ListingView& listing)
    {
        return listing.Status == 0 || listing.Status == 3;
    }
}

CNewUIAuctionHouse::CNewUIAuctionHouse()
{
    m_pNewUIMng = NULL;
    m_pNewUI3DRenderMng = NULL;
    m_Pos.x = 0;
    m_Pos.y = 0;
    m_CurrentView = 0;
    m_CurrentPage = 1;
    m_Filter = 0;
    m_SellCurrency = 1;
    m_SellJewelSlot = 0;
    m_CreateSlot = -1;
    m_CreateItemType = -1;
    m_CreateItemLevel = 0;
    m_SelectedRow = -1;
    m_HoveredRow = -1;
    m_RowCount = 0;
    m_StatusMessage[0] = L'\0';
    m_StatusMessage2[0] = L'\0';
    ZeroMemory(m_Listings, sizeof(m_Listings));
}

CNewUIAuctionHouse::~CNewUIAuctionHouse()
{
    Release();
}

bool CNewUIAuctionHouse::Create(CNewUIManager* pNewUIMng, CNewUI3DRenderMng* pNewUI3DRenderMng, int x, int y)
{
    if (NULL == pNewUIMng || NULL == pNewUI3DRenderMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(INTERFACE_AUCTIONHOUSE, this);

    m_pNewUI3DRenderMng = pNewUI3DRenderMng;
    m_pNewUI3DRenderMng->Add3DRenderObj(this, INFORMATION_CAMERA_Z_ORDER);

    SetPos(10, 70);
    LoadImages();
    InitButtons();
    Show(false);

    return true;
}

void CNewUIAuctionHouse::Release()
{
    UnloadImages();

    if (m_pNewUI3DRenderMng)
    {
        m_pNewUI3DRenderMng->Remove3DRenderObj(this);
        m_pNewUI3DRenderMng = NULL;
    }

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

void CNewUIAuctionHouse::SetPos(int x, int y)
{
    m_Pos.x = x;
    m_Pos.y = y;
}

void CNewUIAuctionHouse::LoadImages()
{
    LoadBitmap(L"Interface\\newui_item_table01(L).tga", IMAGE_TABLE_TOP_LEFT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table01(R).tga", IMAGE_TABLE_TOP_RIGHT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table02(L).tga", IMAGE_TABLE_BOTTOM_LEFT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table02(R).tga", IMAGE_TABLE_BOTTOM_RIGHT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table03(Up).tga", IMAGE_TABLE_TOP_PIXEL, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table03(Dw).tga", IMAGE_TABLE_BOTTOM_PIXEL, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table03(L).tga", IMAGE_TABLE_LEFT_PIXEL, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table03(R).tga", IMAGE_TABLE_RIGHT_PIXEL, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_Btn_round.tga", IMAGE_ROUND_BUTTON, GL_LINEAR);
    LoadBitmap(L"Interface\\InGameShop\\Ingame_Bt03.tga", IMAGE_IGS_BUTTON, GL_LINEAR, GL_CLAMP, 1, 0);
}

void CNewUIAuctionHouse::UnloadImages()
{
    DeleteBitmap(IMAGE_ROUND_BUTTON);
    DeleteBitmap(IMAGE_IGS_BUTTON);
    DeleteBitmap(IMAGE_TABLE_RIGHT_PIXEL);
    DeleteBitmap(IMAGE_TABLE_LEFT_PIXEL);
    DeleteBitmap(IMAGE_TABLE_BOTTOM_PIXEL);
    DeleteBitmap(IMAGE_TABLE_TOP_PIXEL);
    DeleteBitmap(IMAGE_TABLE_BOTTOM_RIGHT);
    DeleteBitmap(IMAGE_TABLE_BOTTOM_LEFT);
    DeleteBitmap(IMAGE_TABLE_TOP_RIGHT);
    DeleteBitmap(IMAGE_TABLE_TOP_LEFT);
}

void CNewUIAuctionHouse::InitButtons()
{
    const int x = m_Pos.x;
    const int y = m_Pos.y;
    SetupAuctionButton(m_BtnHelp, -200, -200, 1, 1, L"?", L"");
    SetupAuctionButton(m_BtnMinimize, -200, -200, 1, 1, L"-", L"");

    int tabX = x + 8;
    SetupAuctionButton(m_BtnBrowse, tabX, y + AUCTION_TAB_Y, 52, 20, L"Browse", L"Browse active auctions");
    tabX += 55;
    SetupAuctionButton(m_BtnMine, tabX, y + AUCTION_TAB_Y, 70, 20, L"My Listings", L"Listings created by you");
    tabX += 73;
    SetupAuctionButton(m_BtnDeliveries, tabX, y + AUCTION_TAB_Y, 52, 20, L"Bought", L"Items you bought");
    tabX += 55;
    SetupAuctionButton(m_BtnPayouts, tabX, y + AUCTION_TAB_Y, 58, 20, L"Payouts", L"Sold listings to claim");
    tabX += 61;
    SetupAuctionButton(m_BtnCreate, tabX, y + AUCTION_TAB_Y, 52, 20, L"Create", L"Create a direct-sale listing");

    SetupAuctionButton(m_BtnMyBidsDisabled, -200, -200, 1, 1, L"", L"");
    SetupAuctionButton(m_BtnWatchlistDisabled, -200, -200, 1, 1, L"", L"");
    SetupAuctionButton(m_BtnHistoryDisabled, -200, -200, 1, 1, L"", L"");

    SetupAuctionButton(m_BtnFilterAll, x + AUCTION_FILTER_X + 8, y + AUCTION_FILTER_Y + 38, 56, 17, L"All", L"Show all listings");
    SetupAuctionButton(m_BtnFilterZen, x + AUCTION_FILTER_X + 8, y + AUCTION_FILTER_Y + 58, 56, 17, L"Zen", L"Show Zen listings");
    SetupAuctionButton(m_BtnFilterWCoin, x + AUCTION_FILTER_X + 8, y + AUCTION_FILTER_Y + 78, 56, 17, L"W Coin", L"Show W Coin listings");
    SetupAuctionButton(m_BtnFilterJewel, x + AUCTION_FILTER_X + 8, y + AUCTION_FILTER_Y + 98, 56, 17, L"Jewels", L"Show jewel listings");
    SetupAuctionButton(m_BtnFilterApply, x + AUCTION_FILTER_X + 8, y + AUCTION_FILTER_Y + 124, 56, 17, L"Apply", L"Apply supported filters");
    SetupAuctionButton(m_BtnFilterReset, x + AUCTION_FILTER_X + 8, y + AUCTION_FILTER_Y + 144, 56, 17, L"Reset", L"Reset supported filters");
    SetupAuctionButton(m_BtnAdvancedFiltersDisabled, -200, -200, 1, 1, L"", L"");

    SetupAuctionButton(m_BtnRefresh, x + AUCTION_BOTTOM_X + 8, y + AUCTION_BOTTOM_Y + 5, 54, 18, L"Refresh", L"Refresh current view");
    SetupAuctionButton(m_BtnPrev, x + AUCTION_BOTTOM_X + 66, y + AUCTION_BOTTOM_Y + 5, 36, 18, L"Prev", L"Previous page");
    SetupAuctionButton(m_BtnNext, x + AUCTION_BOTTOM_X + 146, y + AUCTION_BOTTOM_Y + 5, 36, 18, L"Next", L"Next page");
    SetupAuctionButton(m_BtnBuy, x + AUCTION_DETAILS_X + 12, y + AUCTION_DETAILS_Y + 154, 98, 24, L"Buy", L"Buy selected browse listing");
    SetupAuctionButton(m_BtnCancelListing, x + AUCTION_DETAILS_X + 12, y + AUCTION_DETAILS_Y + 154, 98, 24, L"Cancel", L"Cancel selected own listing");
    SetupAuctionButton(m_BtnReceiveItem, x + AUCTION_DETAILS_X + 12, y + AUCTION_DETAILS_Y + 154, 98, 24, L"Receive", L"Receive selected bought item");
    SetupAuctionButton(m_BtnClaimPayout, x + AUCTION_DETAILS_X + 12, y + AUCTION_DETAILS_Y + 154, 98, 24, L"Claim", L"Claim selected seller payout");
    SetupAuctionButton(m_BtnPlaceBidDisabled, -200, -200, 1, 1, L"", L"");
    SetupAuctionButton(m_BtnCompareDisabled, -200, -200, 1, 1, L"", L"");
    SetupAuctionButton(m_BtnReportDisabled, -200, -200, 1, 1, L"", L"");

    SetupAuctionButton(m_BtnSellZen, x + AUCTION_CREATE_X + 176, y + AUCTION_CREATE_Y + 68, 44, 18, L"Zen", L"Price in Zen");
    SetupAuctionButton(m_BtnSellWCoin, x + AUCTION_CREATE_X + 224, y + AUCTION_CREATE_Y + 68, 52, 18, L"W Coin", L"Price in W Coin");
    SetupAuctionButton(m_BtnSellJewel, x + AUCTION_CREATE_X + 280, y + AUCTION_CREATE_Y + 68, 50, 18, L"Jewel", L"Price in Jewel Bank currency");
    SetupAuctionButton(m_BtnSellJewelType, x + AUCTION_CREATE_X + 176, y + AUCTION_CREATE_Y + 90, 154, 18, L"Bless", L"Cycle jewel currency type");
    SetupAuctionButton(m_BtnAddItem, x + AUCTION_CREATE_X + 16, y + AUCTION_CREATE_Y + 112, 64, 18, L"Add Item", L"Use selected inventory item");
    SetupAuctionButton(m_BtnClearItem, x + AUCTION_CREATE_X + 84, y + AUCTION_CREATE_Y + 112, 56, 18, L"Clear", L"Clear selected item");
    SetupAuctionButton(m_BtnPostListing, x + AUCTION_CREATE_X + 232, y + AUCTION_CREATE_Y + 112, 78, 22, L"Post", L"Create listing");

    m_BtnClose.ChangeButtonImgState(1, IMAGE_BASE_WINDOW_BTN_EXIT, 0, 0, 0);
    m_BtnClose.ChangeButtonInfo(x + WINDOW_WIDTH - 42, y + 4, 36, 29);
    m_BtnClose.ChangeText(L"");
    m_BtnClose.ChangeToolTipText(L"Close", TRUE);

    m_PriceInput.Init(g_hWnd, 80, 16, 10, false);
    m_PriceInput.SetPosition(x + AUCTION_CREATE_X + 190, y + AUCTION_CREATE_Y + 40);
    m_PriceInput.SetTextColor(255, 230, 220, 190);
    m_PriceInput.SetBackColor(170, 6, 7, 7);
    m_PriceInput.SetSelectBackColor(180, 42, 86, 145);
    m_PriceInput.SetOption(UIOPTION_NUMBERONLY | UIOPTION_ENTERIMECHKOFF);
    m_PriceInput.SetFont(g_hFont);
    m_PriceInput.SetState(UISTATE_HIDE);
    m_PriceInput.SetText(L"100");
}

float CNewUIAuctionHouse::GetLayerDepth()
{
    return 3.4f;
}

float CNewUIAuctionHouse::GetKeyEventOrder()
{
    return 3.4f;
}

void CNewUIAuctionHouse::SetListingsHeader(BYTE view, BYTE page, BYTE count)
{
    m_CurrentView = view;
    m_CurrentPage = page == 0 ? 1 : page;
    m_RowCount = 0;
    m_SelectedRow = -1;
    m_HoveredRow = -1;
    ZeroMemory(m_Listings, sizeof(m_Listings));
    std::swprintf(m_StatusMessage, 128, L"%u result(s)", count);
    m_StatusMessage2[0] = L'\0';
}

void CNewUIAuctionHouse::AddListing(const ListingView& listing)
{
    if (m_RowCount >= MAX_ROWS)
        return;

    m_Listings[m_RowCount] = listing;
    m_RowCount++;
}

void CNewUIAuctionHouse::SetStatusMessage(const wchar_t* message)
{
    if (message == NULL)
        return;

    wcsncpy(m_StatusMessage, message, 127);
    m_StatusMessage[127] = L'\0';
    m_StatusMessage2[0] = L'\0';
}

bool CNewUIAuctionHouse::IsCreateListingView() const
{
    return IsVisible() && m_CurrentView == AUCTION_VIEW_CREATE;
}

bool CNewUIAuctionHouse::TrySetCreateListingItemFromInventorySlot(int slot)
{
    if (!IsCreateListingView())
    {
        return false;
    }

    if (slot < MAX_EQUIPMENT_INDEX || slot >= MAX_MY_INVENTORY_EX_INDEX)
    {
        return false;
    }

    ITEM* item = g_pMyInventory != NULL ? g_pMyInventory->FindItem(slot) : NULL;
    m_CreateSlot = slot;
    if (item != NULL)
    {
        m_CreateItemType = item->Type;
        m_CreateItemLevel = item->Level;
    }
    else
    {
        m_CreateItemType = -1;
        m_CreateItemLevel = 0;
    }

    wchar_t message[128] = { 0 };
    std::swprintf(message, 128, L"Item selected for listing: slot %d", slot);
    SetStatusMessage(message);
    return true;
}

void CNewUIAuctionHouse::SetStatusMessages(const wchar_t* line1, const wchar_t* line2)
{
    if (line1 == NULL)
        line1 = L"";
    if (line2 == NULL)
        line2 = L"";

    wcsncpy(m_StatusMessage, line1, 127);
    m_StatusMessage[127] = L'\0';
    wcsncpy(m_StatusMessage2, line2, 127);
    m_StatusMessage2[127] = L'\0';
}

void CNewUIAuctionHouse::SendRequest(BYTE op, BYTE arg1, BYTE currency, BYTE jewelSlot, unsigned int arg2, unsigned int arg3)
{
    if (SocketClient == NULL)
        return;

    SocketClient->ToGameServer()->SendAuctionHouseRequest(op, arg1, currency, jewelSlot, arg2, arg3);
}

void CNewUIAuctionHouse::RequestCurrentView()
{
    switch (m_CurrentView)
    {
    case AUCTION_VIEW_MINE:
        SendRequest(3, 0, 0, 0xFF, 0, 0);
        break;
    case AUCTION_VIEW_DELIVERIES:
        SendRequest(5, 0, 0, 0xFF, 0, 0);
        break;
    case AUCTION_VIEW_PAYOUTS:
        SendRequest(7, 0, 0, 0xFF, 0, 0);
        break;
    case AUCTION_VIEW_CREATE:
        SetStatusMessage(L"Create listing: add a backpack item, set price, then post.");
        break;
    default:
        SendRequest(0, m_CurrentPage, m_Filter, 0xFF, 0, 0);
        break;
    }
}

void CNewUIAuctionHouse::SetView(BYTE view)
{
    m_CurrentView = view;
    m_CurrentPage = 1;
    m_SelectedRow = -1;
    m_HoveredRow = -1;
    if (view == AUCTION_VIEW_CREATE)
    {
        m_RowCount = 0;
        ZeroMemory(m_Listings, sizeof(m_Listings));
        m_PriceInput.SetState(UISTATE_NORMAL);
    }
    else
    {
        m_PriceInput.SetState(UISTATE_HIDE);
        SetRelatedWnd(g_hWnd);
    }
    RequestCurrentView();
}

void CNewUIAuctionHouse::Toggle()
{
    if (IsVisible())
    {
        m_PriceInput.SetState(UISTATE_HIDE);
        SetRelatedWnd(g_hWnd);
        Show(false);
        return;
    }

    if (g_pNewUIMailbox && g_pNewUIMailbox->IsVisible())
    {
        g_pNewUISystem->Hide(INTERFACE_MAILBOX);
    }

    Show(true);
    SetRelatedWnd(g_hWnd);
    SetView(0);
}

void CNewUIAuctionHouse::SetFilter(BYTE filter)
{
    m_Filter = filter;
    m_CurrentPage = 1;
    SetView(0);
}

void CNewUIAuctionHouse::ClearCreateSelection()
{
    m_CreateSlot = -1;
    m_CreateItemType = -1;
    m_CreateItemLevel = 0;
}

void CNewUIAuctionHouse::AddSelectedInventoryItemToCreateListing()
{
    const int slot = GetSelectedInventorySlot();
    ITEM* item = slot >= 0 && g_pMyInventory != NULL ? g_pMyInventory->FindItem(slot) : NULL;
    if (slot < 0 || slot > 255 || item == NULL)
    {
        SetStatusMessage(L"Open inventory, select an item, then press Add Item.");
        return;
    }

    m_CreateSlot = slot;
    m_CreateItemType = item->Type;
    m_CreateItemLevel = item->Level;
    SetStatusMessage(L"Item selected for listing.");
}

bool CNewUIAuctionHouse::SendSelectedListingAction(BYTE op, const wchar_t* action)
{
    if (m_SelectedRow < 0 || m_SelectedRow >= m_RowCount)
    {
        SetStatusMessage(L"No listing selected");
        return true;
    }

    const ListingView listing = m_Listings[m_SelectedRow];
    if (listing.ListingNumber == 0)
    {
        SetStatusMessage(L"Invalid listing number");
        return true;
    }

    wchar_t selectedMessage[128] = { 0 };
    std::swprintf(
        selectedMessage,
        128,
        L"Selected idx=%d list=%u st=%u cur=%u price=%u jewel=%u",
        m_SelectedRow,
        listing.ListingNumber,
        static_cast<unsigned int>(listing.Status),
        static_cast<unsigned int>(listing.Currency),
        listing.Price,
        static_cast<unsigned int>(listing.JewelSlot));
    g_ConsoleDebug->Write(MCD_NORMAL, L"[Auction House] %ls", selectedMessage);

    switch (op)
    {
    case 2:
    {
        wchar_t message[128] = { 0 };
        std::swprintf(message, 128, L"Sending op 2 list=%u price=%u cur=%u jewel=%u", listing.ListingNumber, listing.Price, listing.Currency, listing.JewelSlot);
        SetStatusMessages(selectedMessage, message);
        g_ConsoleDebug->Write(MCD_NORMAL, L"[Auction House] %ls", message);
        SendRequest(2, 0, listing.Currency, listing.JewelSlot, listing.ListingNumber, listing.Price);
        return true;
    }
    case 4:
    {
        if (!IsAuctionListingCancellable(listing))
        {
            SetStatusMessage(L"Action unavailable in this view");
            return true;
        }

        wchar_t message[128] = { 0 };
        std::swprintf(message, 128, L"Sending op 4 list=%u", listing.ListingNumber);
        SetStatusMessages(selectedMessage, message);
        g_ConsoleDebug->Write(MCD_NORMAL, L"[Auction House] %ls", message);
        SendRequest(4, 0, 0, 0xFF, listing.ListingNumber, 0);
        return true;
    }
    case 6:
    {
        wchar_t message[128] = { 0 };
        std::swprintf(message, 128, L"Sending op 6 list=%u", listing.ListingNumber);
        SetStatusMessages(selectedMessage, message);
        g_ConsoleDebug->Write(MCD_NORMAL, L"[Auction House] %ls", message);
        SendRequest(6, 0, 0, 0xFF, listing.ListingNumber, 0);
        return true;
    }
    case 8:
    {
        wchar_t message[128] = { 0 };
        std::swprintf(message, 128, L"Sending op 8 list=%u", listing.ListingNumber);
        SetStatusMessages(selectedMessage, message);
        g_ConsoleDebug->Write(MCD_NORMAL, L"[Auction House] %ls", message);
        SendRequest(8, 0, 0, 0xFF, listing.ListingNumber, 0);
        return true;
    }
    default:
        SetStatusMessage(L"Action unavailable in this view");
        return true;
    }
}

bool CNewUIAuctionHouse::SendCreateListingAction()
{
    unsigned int price = 0;
    if (m_CreateSlot < 0 || m_CreateSlot > 255)
    {
        SetStatusMessage(L"Invalid inventory slot");
        return true;
    }

    if (!TryGetSellPrice(price))
    {
        SetStatusMessage(L"Invalid price");
        return true;
    }

    const BYTE jewelSlot = m_SellCurrency == 2 ? m_SellJewelSlot : 0xFF;
    wchar_t postMessage[128] = { 0 };
    wchar_t sendMessage[128] = { 0 };
    std::swprintf(postMessage, 128, L"POST slot=%d price=%u cur=%u jewel=%u", m_CreateSlot, price, m_SellCurrency, jewelSlot);
    std::swprintf(sendMessage, 128, L"Sending op 1 slot=%d price=%u cur=%u jewel=%u", m_CreateSlot, price, m_SellCurrency, jewelSlot);
    SetStatusMessages(postMessage, sendMessage);
    g_ConsoleDebug->Write(MCD_NORMAL, L"[Auction House] %ls", postMessage);
    g_ConsoleDebug->Write(MCD_NORMAL, L"[Auction House] %ls", sendMessage);

    SendRequest(1, static_cast<BYTE>(m_CreateSlot), m_SellCurrency, jewelSlot, price, 0);
    ClearCreateSelection();
    return true;
}

bool CNewUIAuctionHouse::ProcessMouseButtons()
{
    if (m_BtnClose.UpdateMouseEvent())
    {
        m_PriceInput.SetState(UISTATE_HIDE);
        SetRelatedWnd(g_hWnd);
        g_pNewUISystem->Hide(INTERFACE_AUCTIONHOUSE);
        return true;
    }

    if (m_BtnBrowse.UpdateMouseEvent())
    {
        SetView(AUCTION_VIEW_BROWSE);
        return true;
    }
    if (m_BtnMine.UpdateMouseEvent())
    {
        SetView(AUCTION_VIEW_MINE);
        return true;
    }
    if (m_BtnDeliveries.UpdateMouseEvent())
    {
        SetView(AUCTION_VIEW_DELIVERIES);
        return true;
    }
    if (m_BtnPayouts.UpdateMouseEvent())
    {
        SetView(AUCTION_VIEW_PAYOUTS);
        return true;
    }
    if (m_BtnCreate.UpdateMouseEvent())
    {
        SetView(AUCTION_VIEW_CREATE);
        return true;
    }

    if (m_BtnFilterAll.UpdateMouseEvent())
    {
        SetFilter(0);
        return true;
    }
    if (m_BtnFilterZen.UpdateMouseEvent())
    {
        SetFilter(1);
        return true;
    }
    if (m_BtnFilterWCoin.UpdateMouseEvent())
    {
        SetFilter(2);
        return true;
    }
    if (m_BtnFilterJewel.UpdateMouseEvent())
    {
        SetFilter(3);
        return true;
    }
    if (m_BtnFilterApply.UpdateMouseEvent())
    {
        SetView(AUCTION_VIEW_BROWSE);
        return true;
    }
    if (m_BtnFilterReset.UpdateMouseEvent())
    {
        SetFilter(0);
        return true;
    }

    if (m_CurrentView != AUCTION_VIEW_CREATE)
    {
        if (m_BtnRefresh.UpdateMouseEvent())
        {
            RequestCurrentView();
            return true;
        }
        if (m_BtnPrev.UpdateMouseEvent())
        {
            if (m_CurrentView == AUCTION_VIEW_BROWSE && m_CurrentPage > 1)
            {
                m_CurrentPage--;
                RequestCurrentView();
            }
            else
            {
                SetStatusMessage(L"Action unavailable in this view");
            }

            return true;
        }
        if (m_BtnNext.UpdateMouseEvent())
        {
            if (m_CurrentView == AUCTION_VIEW_BROWSE)
            {
                m_CurrentPage++;
                RequestCurrentView();
            }
            else
            {
                SetStatusMessage(L"Action unavailable in this view");
            }

            return true;
        }
    }

    if (m_CurrentView == AUCTION_VIEW_CREATE)
    {
        if (m_BtnSellZen.UpdateMouseEvent())
        {
            m_SellCurrency = 0;
            SetStatusMessage(L"Create listing currency: Zen.");
            return true;
        }
        if (m_BtnSellWCoin.UpdateMouseEvent())
        {
            m_SellCurrency = 1;
            SetStatusMessage(L"Create listing currency: W Coin.");
            return true;
        }
        if (m_BtnSellJewel.UpdateMouseEvent())
        {
            m_SellCurrency = 2;
            if (m_SellJewelSlot > 16)
                m_SellJewelSlot = 0;
            SetStatusMessage(L"Create listing currency: Jewel Bank.");
            return true;
        }
        if (m_BtnSellJewelType.UpdateMouseEvent())
        {
            m_SellCurrency = 2;
            m_SellJewelSlot = (m_SellJewelSlot + 1) % 17;
            SetStatusMessage(L"Jewel currency type changed.");
            return true;
        }
        if (m_BtnAddItem.UpdateMouseEvent())
        {
            AddSelectedInventoryItemToCreateListing();
            return true;
        }
        if (m_BtnClearItem.UpdateMouseEvent())
        {
            ClearCreateSelection();
            SetStatusMessage(L"Create listing cleared.");
            return true;
        }
        if (m_BtnPostListing.UpdateMouseEvent())
        {
            return SendCreateListingAction();
        }

        return false;
    }

    if (m_CurrentView == AUCTION_VIEW_BROWSE && m_BtnBuy.UpdateMouseEvent())
        return SendSelectedListingAction(2, L"BUY");
    if (m_CurrentView == AUCTION_VIEW_MINE && m_BtnCancelListing.UpdateMouseEvent())
        return SendSelectedListingAction(4, L"CANCEL");
    if (m_CurrentView == AUCTION_VIEW_DELIVERIES && m_BtnReceiveItem.UpdateMouseEvent())
        return SendSelectedListingAction(6, L"RECEIVE");
    if (m_CurrentView == AUCTION_VIEW_PAYOUTS && m_BtnClaimPayout.UpdateMouseEvent())
        return SendSelectedListingAction(8, L"CLAIM");

    return false;
}

bool CNewUIAuctionHouse::TryGetSellPrice(unsigned int& price) const
{
    wchar_t text[32] = { 0 };
    const_cast<CUITextInputBox&>(m_PriceInput).GetText(text, 32);
    wchar_t* end = nullptr;
    const unsigned long parsed = std::wcstoul(text, &end, 10);
    if (parsed == 0 || parsed > 2000000000UL)
    {
        return false;
    }

    price = static_cast<unsigned int>(parsed);
    return true;
}

int CNewUIAuctionHouse::GetSelectedInventorySlot() const
{
    if (g_pMyInventory == NULL)
    {
        return -1;
    }

    CNewUIPickedItem* picked = CNewUIInventoryCtrl::GetPickedItem();
    if (picked != NULL && picked->GetOwnerInventory() == g_pMyInventory->GetInventoryCtrl())
    {
        return picked->GetSourceLinealPos();
    }

    return g_pMyInventory->GetPointedItemIndex();
}

bool CNewUIAuctionHouse::Update()
{
    if (!IsVisible())
        return true;

    m_PriceInput.DoAction();
    if (m_PriceInput.HaveFocus())
    {
        SetRelatedWnd(m_PriceInput.GetHandle());
    }
    else
    {
        SetRelatedWnd(g_hWnd);
    }

    return true;
}

bool CNewUIAuctionHouse::UpdateMouseEvent()
{
    if (!CheckMouseIn(m_Pos.x, m_Pos.y, WINDOW_WIDTH, WINDOW_HEIGHT))
        return true;

    m_HoveredRow = -1;
    if (ProcessMouseButtons())
        return false;

    if (m_CurrentView != AUCTION_VIEW_CREATE)
    {
        const int tableX = m_Pos.x + AUCTION_TABLE_X;
        const int tableY = m_Pos.y + AUCTION_TABLE_Y;
        for (int i = 0; i < m_RowCount; ++i)
        {
            const int rowY = tableY + AUCTION_HEADER_HEIGHT + i * AUCTION_ROW_HEIGHT;
            if (CheckMouseIn(tableX, rowY, AUCTION_TABLE_WIDTH, AUCTION_ROW_HEIGHT))
            {
                m_HoveredRow = i;
                if (IsRelease(VK_LBUTTON))
                {
                    m_SelectedRow = i;
                    return false;
                }
            }
        }
    }

    return false;
}

bool CNewUIAuctionHouse::UpdateKeyEvent()
{
    if (IsVisible() && IsPress(VK_ESCAPE) == true)
    {
        m_PriceInput.SetState(UISTATE_HIDE);
        SetRelatedWnd(g_hWnd);
        g_pNewUISystem->Hide(INTERFACE_AUCTIONHOUSE);
        return false;
    }

    return true;
}

bool CNewUIAuctionHouse::IsVisible() const
{
    return CNewUIObj::IsVisible();
}

void CNewUIAuctionHouse::RenderBack()
{
    RenderJewelBankRect(m_Pos.x, m_Pos.y, WINDOW_WIDTH, WINDOW_HEIGHT, 0.010f, 0.012f, 0.015f, 0.96f);
    RenderJewelBankRect(m_Pos.x + 2, m_Pos.y + 2, WINDOW_WIDTH - 4, WINDOW_HEIGHT - 4, 0.035f, 0.040f, 0.048f, 0.94f);
    RenderJewelBankRect(m_Pos.x + 6, m_Pos.y + 6, WINDOW_WIDTH - 12, 24, 0.060f, 0.064f, 0.072f, 0.94f);
    RenderJewelBankLine(m_Pos.x + 6, m_Pos.y + 31, WINDOW_WIDTH - 12, 1, 0.38f, 0.34f, 0.24f, 0.62f);

    g_pRenderText->SetFont(g_hFontBold);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(238, 210, 142, 255);
    g_pRenderText->RenderText(m_Pos.x, m_Pos.y + 13, L"AUCTION HOUSE", WINDOW_WIDTH, 0, RT3_SORT_CENTER);
}

void CNewUIAuctionHouse::RenderTable()
{
    const int panelX = m_Pos.x + AUCTION_TABLE_PANEL_X;
    const int panelY = m_Pos.y + AUCTION_TABLE_PANEL_Y;
    const int tableX = m_Pos.x + AUCTION_TABLE_X;
    const int tableY = m_Pos.y + AUCTION_TABLE_Y;

    RenderAuctionPanel(panelX, panelY, AUCTION_TABLE_PANEL_WIDTH, AUCTION_TABLE_PANEL_HEIGHT, L"LISTINGS");
    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(156, 184, 218, 255);
    g_pRenderText->RenderText(panelX + 8, panelY + 23, GetViewTitle(), AUCTION_TABLE_PANEL_WIDTH - 16, 0, RT3_SORT_LEFT);
    g_pRenderText->SetTextColor(196, 177, 125, 255);
    wchar_t pageText[32] = { 0 };
    std::swprintf(pageText, 32, L"Page %u", m_CurrentPage);
    g_pRenderText->RenderText(panelX + AUCTION_TABLE_PANEL_WIDTH - 72, panelY + 23, pageText, 64, 0, RT3_SORT_RIGHT);

    RenderJewelBankRect(tableX, tableY, AUCTION_TABLE_WIDTH, AUCTION_HEADER_HEIGHT, 0.075f, 0.084f, 0.094f, 0.94f);
    RenderJewelBankLine(tableX, tableY, AUCTION_TABLE_WIDTH, 1, 0.47f, 0.40f, 0.25f, 0.54f);
    RenderJewelBankLine(tableX, tableY + AUCTION_HEADER_HEIGHT, AUCTION_TABLE_WIDTH, 1, 0.23f, 0.25f, 0.27f, 0.70f);

    for (int i = 0; i <= MAX_ROWS; ++i)
    {
        const int rowY = tableY + AUCTION_HEADER_HEIGHT + i * AUCTION_ROW_HEIGHT;
        RenderJewelBankLine(tableX, rowY, AUCTION_TABLE_WIDTH, 1, 0.16f, 0.17f, 0.18f, 0.62f);
    }

    for (int i = 0; i < AUCTION_COLUMN_COUNT; ++i)
    {
        const int columnX = tableX + s_AuctionColumns[i];
        RenderJewelBankLine(columnX, tableY, 1, AUCTION_HEADER_HEIGHT + MAX_ROWS * AUCTION_ROW_HEIGHT, 0.18f, 0.19f, 0.20f, 0.66f);
    }

    for (int i = 0; i < MAX_ROWS; ++i)
    {
        const int rowY = tableY + AUCTION_HEADER_HEIGHT + i * AUCTION_ROW_HEIGHT;
        const bool alt = (i % 2) != 0;
        RenderJewelBankRect(tableX + 1, rowY + 1, AUCTION_TABLE_WIDTH - 2, AUCTION_ROW_HEIGHT - 1,
            alt ? 0.045f : 0.060f, alt ? 0.050f : 0.060f, alt ? 0.058f : 0.070f, 0.72f);
    }

    for (int i = 0; i < m_RowCount; ++i)
    {
        const int rowY = tableY + AUCTION_HEADER_HEIGHT + i * AUCTION_ROW_HEIGHT;
        if (i == m_HoveredRow)
        {
            RenderJewelBankRect(tableX + 1, rowY + 1, AUCTION_TABLE_WIDTH - 2, AUCTION_ROW_HEIGHT - 2, 0.02f, 0.18f, 0.34f, 0.42f);
        }
        if (i == m_SelectedRow)
        {
            RenderJewelBankRect(tableX + 1, rowY + 1, AUCTION_TABLE_WIDTH - 2, AUCTION_ROW_HEIGHT - 2, 0.02f, 0.27f, 0.55f, 0.58f);
            RenderJewelBankLine(tableX + 1, rowY + 1, AUCTION_TABLE_WIDTH - 2, 1, 0.14f, 0.58f, 1.00f, 0.92f);
        }
    }

    for (int i = 0; i <= MAX_ROWS; ++i)
    {
        const int rowY = tableY + AUCTION_HEADER_HEIGHT + i * AUCTION_ROW_HEIGHT;
        RenderJewelBankLine(tableX, rowY, AUCTION_TABLE_WIDTH, 1, 0.18f, 0.19f, 0.20f, 0.72f);
    }

    for (int i = 0; i < AUCTION_COLUMN_COUNT; ++i)
    {
        const int columnX = tableX + s_AuctionColumns[i];
        RenderJewelBankLine(columnX, tableY, 1, AUCTION_HEADER_HEIGHT + MAX_ROWS * AUCTION_ROW_HEIGHT, 0.18f, 0.19f, 0.20f, 0.70f);
    }

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(222, 224, 220, 255);
    for (int i = 0; i < AUCTION_COLUMN_COUNT - 1; ++i)
    {
        const int columnX = tableX + s_AuctionColumns[i];
        const int columnWidth = s_AuctionColumns[i + 1] - s_AuctionColumns[i];
        g_pRenderText->RenderText(columnX, tableY + 4, s_AuctionHeaders[i], columnWidth, 0, RT3_SORT_CENTER);
    }

    for (int i = 0; i < m_RowCount; ++i)
    {
        const ListingView& listing = m_Listings[i];
        const int rowY = tableY + AUCTION_HEADER_HEIGHT + i * AUCTION_ROW_HEIGHT;
        wchar_t price[32] = { 0 };
        wchar_t level[16] = { 0 };
        std::swprintf(price, 32, L"%u", listing.Price);
        std::swprintf(level, 16, L"+%u", listing.ItemLevel);
        const wchar_t* currencyText = L"?";
        if (listing.Currency == 0)
            currencyText = L"Zen";
        else if (listing.Currency == 1)
            currencyText = L"W";
        else if (listing.Currency == 2)
            currencyText = L"Jwl";

        g_pRenderText->SetTextColor(235, 202, 104, 255);
        g_pRenderText->RenderText(tableX + s_AuctionColumns[0] + 18, rowY + 3, listing.ItemName, s_AuctionColumns[1] - s_AuctionColumns[0] - 20, 0, RT3_SORT_LEFT);
        g_pRenderText->SetTextColor(216, 220, 224, 255);
        g_pRenderText->RenderText(tableX + s_AuctionColumns[1], rowY + 3, level, s_AuctionColumns[2] - s_AuctionColumns[1], 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(tableX + s_AuctionColumns[2], rowY + 3, listing.SellerName, s_AuctionColumns[3] - s_AuctionColumns[2], 0, RT3_SORT_CENTER);
        g_pRenderText->SetTextColor(236, 176, 96, 255);
        g_pRenderText->RenderText(tableX + s_AuctionColumns[3], rowY + 3, currencyText, s_AuctionColumns[4] - s_AuctionColumns[3], 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(tableX + s_AuctionColumns[4], rowY + 3, price, s_AuctionColumns[5] - s_AuctionColumns[4], 0, RT3_SORT_CENTER);
    }

    if (m_RowCount == 0)
    {
        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(130, 138, 148, 255);
        g_pRenderText->RenderText(tableX, tableY + 104, L"No listings in this view.", AUCTION_TABLE_WIDTH, 0, RT3_SORT_CENTER);
    }
}

void CNewUIAuctionHouse::RenderFilters()
{
    const int x = m_Pos.x + AUCTION_FILTER_X;
    const int y = m_Pos.y + AUCTION_FILTER_Y;

    RenderAuctionPanel(x, y, AUCTION_FILTER_WIDTH, AUCTION_FILTER_HEIGHT, L"FILTERS");

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(212, 214, 214, 255);
    g_pRenderText->RenderText(x + 8, y + 25, L"Currency", AUCTION_FILTER_WIDTH - 16, 0, RT3_SORT_LEFT);

    RenderAuctionButton(m_BtnFilterAll, L"All", true, m_Filter == 0 ? AUCTION_TONE_BUY : AUCTION_TONE_NEUTRAL);
    RenderAuctionButton(m_BtnFilterZen, L"Zen", true, m_Filter == 1 ? AUCTION_TONE_BUY : AUCTION_TONE_NEUTRAL);
    RenderAuctionButton(m_BtnFilterWCoin, L"W Coin", true, m_Filter == 2 ? AUCTION_TONE_BUY : AUCTION_TONE_NEUTRAL);
    RenderAuctionButton(m_BtnFilterJewel, L"Jewels", true, m_Filter == 3 ? AUCTION_TONE_BUY : AUCTION_TONE_NEUTRAL);

    g_pRenderText->SetTextColor(166, 198, 245, 255);
    const wchar_t* filterText = L"All currencies";
    if (m_Filter == 1)
        filterText = L"Zen only";
    else if (m_Filter == 2)
        filterText = L"W Coin only";
    else if (m_Filter == 3)
        filterText = L"Jewels only";
    g_pRenderText->RenderText(x + 8, y + 166, L"Status", AUCTION_FILTER_WIDTH - 16, 0, RT3_SORT_LEFT);
    g_pRenderText->SetTextColor(232, 190, 84, 255);
    g_pRenderText->RenderText(x + 8, y + 178, filterText, AUCTION_FILTER_WIDTH - 16, 0, RT3_SORT_LEFT);

    RenderAuctionButton(m_BtnFilterApply, L"Apply", true, AUCTION_TONE_CONFIRM);
    RenderAuctionButton(m_BtnFilterReset, L"Reset", true, AUCTION_TONE_NEUTRAL);
}

void CNewUIAuctionHouse::RenderDetails()
{
    const int x = m_Pos.x + AUCTION_DETAILS_X;
    const int y = m_Pos.y + AUCTION_DETAILS_Y;
    const ListingView* listing = (m_SelectedRow >= 0 && m_SelectedRow < m_RowCount) ? &m_Listings[m_SelectedRow] : NULL;

    RenderAuctionPanel(x, y, AUCTION_DETAILS_WIDTH, AUCTION_DETAILS_HEIGHT, L"LISTING DETAILS");

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    if (listing == NULL)
    {
        g_pRenderText->SetTextColor(150, 154, 160, 255);
        g_pRenderText->RenderText(x + 8, y + 70, L"Select a listing", AUCTION_DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(x + 8, y + 84, L"to inspect it.", AUCTION_DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
        if (m_CurrentView == AUCTION_VIEW_MINE)
        {
            g_pRenderText->RenderText(x + 8, y + 104, L"Select one of", AUCTION_DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
            g_pRenderText->RenderText(x + 8, y + 118, L"your listings.", AUCTION_DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
        }
        return;
    }

    wchar_t idText[32] = { 0 };
    wchar_t priceText[48] = { 0 };
    wchar_t typeText[32] = { 0 };
    wchar_t levelText[16] = { 0 };
    wchar_t jewelText[48] = { 0 };
    std::swprintf(idText, 32, L"#%u", listing->ListingNumber);
    std::swprintf(priceText, 48, L"%u %s", listing->Price, GetCurrencyText(listing->Currency, listing->JewelSlot));
    std::swprintf(typeText, 32, L"%u:%u", listing->ItemType / 512, listing->ItemType % 512);
    std::swprintf(levelText, 16, L"+%u", listing->ItemLevel);
    if (listing->Currency == 2 && listing->JewelSlot < 17)
        std::swprintf(jewelText, 48, L"%u - %s", listing->JewelSlot, GetCurrencyText(listing->Currency, listing->JewelSlot));
    else
        std::swprintf(jewelText, 48, L"-");

    g_pRenderText->SetTextColor(86, 236, 86, 255);
    g_pRenderText->RenderText(x + 42, y + 28, listing->ItemName, 74, 0, RT3_SORT_LEFT);
    g_pRenderText->SetTextColor(216, 218, 218, 255);
    g_pRenderText->RenderText(x + 8, y + 54, L"Listing", 40, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 50, y + 54, idText, 66, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 8, y + 66, L"Type", 40, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 50, y + 66, typeText, 66, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 8, y + 78, L"Level", 40, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 50, y + 78, levelText, 66, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 8, y + 90, L"Seller", 40, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 50, y + 90, listing->SellerName, 66, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 8, y + 102, L"Status", 40, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 50, y + 102, GetStatusText(listing->Status), 66, 0, RT3_SORT_LEFT);
    g_pRenderText->SetTextColor(236, 184, 86, 255);
    g_pRenderText->RenderText(x + 8, y + 114, L"Price", 40, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 50, y + 114, priceText, 66, 0, RT3_SORT_LEFT);
    g_pRenderText->SetTextColor(156, 166, 176, 255);
    g_pRenderText->RenderText(x + 8, y + 126, L"Jewel", 40, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 50, y + 126, jewelText, 66, 0, RT3_SORT_LEFT);
}

void CNewUIAuctionHouse::RenderCreateListing()
{
    const int x = m_Pos.x + AUCTION_CREATE_X;
    const int y = m_Pos.y + AUCTION_CREATE_Y;

    RenderAuctionPanel(x, y, AUCTION_CREATE_WIDTH, AUCTION_CREATE_HEIGHT, L"CREATE LISTING");
    RenderJewelBankRect(x + 16, y + 34, 72, 62, 0.010f, 0.012f, 0.014f, 0.94f);
    RenderJewelBankRect(x + 18, y + 36, 68, 58, 0.055f, 0.060f, 0.068f, 0.86f);
    RenderJewelBankLine(x + 18, y + 36, 68, 1, 0.46f, 0.38f, 0.23f, 0.58f);

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    if (m_CreateSlot >= 0)
    {
        wchar_t slotText[32] = { 0 };
        std::swprintf(slotText, 32, L"Slot %d", m_CreateSlot);

        g_pRenderText->SetTextColor(116, 236, 126, 255);
        g_pRenderText->RenderText(x + 16, y + 98, slotText, 72, 0, RT3_SORT_CENTER);
    }
    else
    {
        g_pRenderText->SetTextColor(154, 160, 168, 255);
        g_pRenderText->RenderText(x + 20, y + 54, L"Item slot", 64, 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(x + 20, y + 68, L"empty", 64, 0, RT3_SORT_CENTER);
    }

    g_pRenderText->SetTextColor(220, 220, 210, 255);
    g_pRenderText->RenderText(x + 116, y + 44, L"Price", 58, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 116, y + 72, L"Currency", 58, 0, RT3_SORT_LEFT);

    RenderAuctionButton(m_BtnSellZen, L"Zen", true, m_SellCurrency == 0 ? AUCTION_TONE_BUY : AUCTION_TONE_NEUTRAL);
    RenderAuctionButton(m_BtnSellWCoin, L"W", true, m_SellCurrency == 1 ? AUCTION_TONE_BUY : AUCTION_TONE_NEUTRAL);
    RenderAuctionButton(m_BtnSellJewel, L"Jewel", true, m_SellCurrency == 2 ? AUCTION_TONE_BUY : AUCTION_TONE_NEUTRAL);

    wchar_t jewelText[64] = { 0 };
    if (m_SellCurrency == 2)
        std::swprintf(jewelText, 64, L"%s", GetCurrencyText(2, m_SellJewelSlot));
    else
        std::swprintf(jewelText, 64, L"Jewel type");
    RenderAuctionButton(m_BtnSellJewelType, jewelText, m_SellCurrency == 2, m_SellCurrency == 2 ? AUCTION_TONE_NEUTRAL : AUCTION_TONE_DISABLED);

    m_PriceInput.Render();
    RenderAuctionButton(m_BtnAddItem, L"Add Item", true, AUCTION_TONE_NEUTRAL);
    RenderAuctionButton(m_BtnClearItem, L"Clear", m_CreateSlot >= 0, AUCTION_TONE_NEUTRAL);
    RenderAuctionButton(m_BtnPostListing, L"Post", m_CreateSlot >= 0, AUCTION_TONE_CONFIRM);

    g_pRenderText->SetTextColor(188, 198, 210, 255);
    g_pRenderText->RenderText(x + 16, y + 144, L"Use Add Item, set price/currency, then Post.", AUCTION_CREATE_WIDTH - 32, 0, RT3_SORT_LEFT);
    g_pRenderText->SetTextColor(150, 156, 164, 255);
    g_pRenderText->RenderText(x + 16, y + 160, L"Right-click inventory item to select it.", AUCTION_CREATE_WIDTH - 32, 0, RT3_SORT_LEFT);
}

void CNewUIAuctionHouse::RenderFooter()
{
    RenderAuctionPanel(m_Pos.x + AUCTION_BOTTOM_X, m_Pos.y + AUCTION_BOTTOM_Y, AUCTION_BOTTOM_WIDTH, AUCTION_BOTTOM_HEIGHT, NULL);
    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    wchar_t pageText[32] = { 0 };
    std::swprintf(pageText, 32, L"Page %u", m_CurrentPage);

    g_pRenderText->SetTextColor(196, 177, 125, 255);
    g_pRenderText->RenderText(m_Pos.x + AUCTION_BOTTOM_X + 106, m_Pos.y + AUCTION_BOTTOM_Y + 8, pageText, 38, 0, RT3_SORT_CENTER);
    if (m_StatusMessage2[0] != L'\0')
    {
        g_pRenderText->SetTextColor(230, 190, 90, 255);
        g_pRenderText->RenderText(m_Pos.x + AUCTION_BOTTOM_X + 194, m_Pos.y + AUCTION_BOTTOM_Y + 3, m_StatusMessage, AUCTION_BOTTOM_WIDTH - 200, 0, RT3_SORT_LEFT);
        g_pRenderText->SetTextColor(166, 198, 245, 255);
        g_pRenderText->RenderText(m_Pos.x + AUCTION_BOTTOM_X + 194, m_Pos.y + AUCTION_BOTTOM_Y + 15, m_StatusMessage2, AUCTION_BOTTOM_WIDTH - 200, 0, RT3_SORT_LEFT);
    }
    else
    {
        g_pRenderText->SetTextColor(188, 198, 210, 255);
        g_pRenderText->RenderText(m_Pos.x + AUCTION_BOTTOM_X + 190, m_Pos.y + AUCTION_BOTTOM_Y + 8, L"Status", 42, 0, RT3_SORT_LEFT);
        g_pRenderText->SetTextColor(230, 190, 90, 255);
        g_pRenderText->RenderText(m_Pos.x + AUCTION_BOTTOM_X + 234, m_Pos.y + AUCTION_BOTTOM_Y + 8, m_StatusMessage, AUCTION_BOTTOM_WIDTH - 240, 0, RT3_SORT_LEFT);
    }
}

void CNewUIAuctionHouse::RenderContextAction()
{
    if (m_CurrentView == AUCTION_VIEW_CREATE)
        return;

    const int x = m_Pos.x + AUCTION_DETAILS_X;
    const int y = m_Pos.y + AUCTION_DETAILS_Y;
    const bool hasSelected = m_SelectedRow >= 0 && m_SelectedRow < m_RowCount;
    const ListingView* listing = hasSelected ? &m_Listings[m_SelectedRow] : NULL;

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(156, 166, 176, 255);

    switch (m_CurrentView)
    {
    case AUCTION_VIEW_MINE:
        if (!hasSelected)
        {
            g_pRenderText->RenderText(x + 8, y + 140, L"Select your listing.", AUCTION_DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
        }
        else if (!IsAuctionListingCancellable(*listing))
        {
            g_pRenderText->RenderText(x + 8, y + 140, L"Cannot cancel.", AUCTION_DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
        }
        RenderAuctionButton(m_BtnCancelListing, L"CANCEL", hasSelected && IsAuctionListingCancellable(*listing), AUCTION_TONE_NEUTRAL);
        break;
    case AUCTION_VIEW_DELIVERIES:
        if (!hasSelected)
            g_pRenderText->RenderText(x + 8, y + 140, L"Select bought item.", AUCTION_DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
        RenderAuctionButton(m_BtnReceiveItem, L"RECEIVE", hasSelected, AUCTION_TONE_CONFIRM);
        break;
    case AUCTION_VIEW_PAYOUTS:
        if (!hasSelected)
            g_pRenderText->RenderText(x + 8, y + 140, L"Select payout.", AUCTION_DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
        RenderAuctionButton(m_BtnClaimPayout, L"CLAIM", hasSelected, AUCTION_TONE_CONFIRM);
        break;
    default:
        if (!hasSelected)
            g_pRenderText->RenderText(x + 8, y + 140, L"Select listing.", AUCTION_DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
        RenderAuctionButton(m_BtnBuy, L"BUY", hasSelected, AUCTION_TONE_BUY);
        break;
    }
}

void CNewUIAuctionHouse::RenderListingActionButton(CNewUIButton& button, const wchar_t* text, bool enabled, int tone)
{
    RenderAuctionButton(button, text, enabled, tone);
}

void CNewUIAuctionHouse::Render3D()
{
    if (!IsVisible())
        return;

    if (m_CurrentView == AUCTION_VIEW_CREATE)
    {
        if (m_CreateItemType >= 0)
        {
            glColor4f(1.f, 1.f, 1.f, 1.f);
            RenderItem3D(float(m_Pos.x + AUCTION_CREATE_X + 22), float(m_Pos.y + AUCTION_CREATE_Y + 40), 58.f, 50.f, m_CreateItemType, m_CreateItemLevel, 0, 0, false);
        }
        return;
    }

    const int tableX = m_Pos.x + AUCTION_TABLE_X;
    const int tableY = m_Pos.y + AUCTION_TABLE_Y;
    for (int i = 0; i < m_RowCount; ++i)
    {
        const int rowY = tableY + AUCTION_HEADER_HEIGHT + i * AUCTION_ROW_HEIGHT;
        glColor4f(1.f, 1.f, 1.f, 1.f);
        RenderItem3D(float(tableX + 2), float(rowY + 1), 14.f, 12.f, m_Listings[i].ItemType, m_Listings[i].ItemLevel, 0, 0, false);
    }

    if (m_SelectedRow >= 0 && m_SelectedRow < m_RowCount)
    {
        const ListingView& listing = m_Listings[m_SelectedRow];
        glColor4f(1.f, 1.f, 1.f, 1.f);
        RenderItem3D(float(m_Pos.x + AUCTION_DETAILS_X + 9), float(m_Pos.y + AUCTION_DETAILS_Y + 29), 28.f, 24.f, listing.ItemType, listing.ItemLevel, 0, 0, false);
    }
}

bool CNewUIAuctionHouse::Render()
{
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, 1.f);

    RenderBack();
    RenderAuctionTab(m_BtnBrowse, L"Browse", m_CurrentView == AUCTION_VIEW_BROWSE, true);
    RenderAuctionTab(m_BtnMine, L"My Listings", m_CurrentView == AUCTION_VIEW_MINE, true);
    RenderAuctionTab(m_BtnDeliveries, L"Bought", m_CurrentView == AUCTION_VIEW_DELIVERIES, true);
    RenderAuctionTab(m_BtnPayouts, L"Payouts", m_CurrentView == AUCTION_VIEW_PAYOUTS, true);
    RenderAuctionTab(m_BtnCreate, L"Create", m_CurrentView == AUCTION_VIEW_CREATE, true);

    RenderFilters();
    RenderFooter();
    if (m_CurrentView == AUCTION_VIEW_CREATE)
    {
        RenderCreateListing();
    }
    else
    {
        RenderTable();
        RenderDetails();
        RenderContextAction();
        RenderListingActionButton(m_BtnRefresh, L"Refresh", true, AUCTION_TONE_NEUTRAL);
        RenderListingActionButton(m_BtnPrev, L"Prev", m_CurrentView == AUCTION_VIEW_BROWSE && m_CurrentPage > 1, AUCTION_TONE_NEUTRAL);
        RenderListingActionButton(m_BtnNext, L"Next", m_CurrentView == AUCTION_VIEW_BROWSE, AUCTION_TONE_NEUTRAL);
    }
    m_BtnClose.Render();

    DisableAlphaBlend();
    return true;
}

const wchar_t* CNewUIAuctionHouse::GetCurrencyText(BYTE currency, BYTE jewelSlot) const
{
    switch (currency)
    {
    case 0:
        return L"Zen";
    case 1:
        return L"W Coin";
    case 2:
        return jewelSlot < 17 ? s_AuctionJewelNames[jewelSlot] : L"Jewel";
    default:
        return L"?";
    }
}

const wchar_t* CNewUIAuctionHouse::GetStatusText(BYTE status) const
{
    switch (status)
    {
    case 0:
        return L"Active";
    case 1:
        return L"Sold";
    case 2:
        return L"Cancelled";
    case 3:
        return L"Expired";
    case 4:
        return L"Done";
    default:
        return L"?";
    }
}

const wchar_t* CNewUIAuctionHouse::GetViewTitle() const
{
    switch (m_CurrentView)
    {
    case AUCTION_VIEW_MINE:
        return L"MY LISTINGS";
    case AUCTION_VIEW_DELIVERIES:
        return L"BOUGHT ITEMS";
    case AUCTION_VIEW_PAYOUTS:
        return L"SELLER PAYOUTS";
    case AUCTION_VIEW_CREATE:
        return L"CREATE LISTING";
    default:
        return L"MARKET LISTINGS";
    }
}

const wchar_t* CNewUIAuctionHouse::GetActionText() const
{
    switch (m_CurrentView)
    {
    case AUCTION_VIEW_MINE:
        return L"Cancel Listing";
    case AUCTION_VIEW_DELIVERIES:
        return L"Receive Item";
    case AUCTION_VIEW_PAYOUTS:
        return L"Claim W Coin";
    case AUCTION_VIEW_CREATE:
        return L"Post Listing";
    default:
        return L"Buy Selected";
    }
}

//////////////////////////////////////////////////////////////////////////
