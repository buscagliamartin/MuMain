#include "stdafx.h"

#include <cwchar>

#include "UI/NewUI/JewelBankWindow.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Inventory/NewUIItemMng.h"
#include "Engine/Object/ZzzInventory.h"
#include "Dotnet/Connection.h"

extern Connection* SocketClient;

using namespace SEASON3B;

//////////////////////////////////////////////////////////////////////////
// BarnaMu: CNewUIJewelBank - per-account jewel bank window.
// Extracted verbatim from the reference client's NewUIMuHelper.cpp (Step 3); the clean MuHelper
// mode foundation is intentionally left untouched.
//////////////////////////////////////////////////////////////////////////

namespace
{
    struct JewelBankItemInfo
    {
        const wchar_t* Name;
        int Type;
        int Level;
    };

    constexpr int JEWEL_BANK_TABLE_X = 14;
    constexpr int JEWEL_BANK_TABLE_Y = 36;
    constexpr int JEWEL_BANK_HEADER_HEIGHT = 24;
    constexpr int JEWEL_BANK_TABLE_WIDTH = 442;
    constexpr int JEWEL_BANK_ICON_SIZE = 18;
    constexpr int JEWEL_BANK_ACTION_W = 42; // slimmer flat cell buttons: clear gap to the grid dividers
    constexpr int JEWEL_BANK_ACTION_H = 13; // and a little row breathing room above/below

    // Mode-toggle + Deposit-All are now flat primitive buttons (RenderJewelBankButton), NOT the
    // IGS texture, so the old <=72px anti-overflow cap no longer applies (a primitive fill has no
    // texture to overflow). They're sized to fit their captions with comfortable padding:
    //   - toggle ("DEPOSIT"/"WITHDRAW") sits centred inside the 106px D+E header span,
    //   - Deposit-All ("DEPOSIT ALL") is a wider, prominent bottom button.
    constexpr int JEWEL_BANK_TOGGLE_W = 92;
    constexpr int JEWEL_BANK_TOGGLE_H = 22;
    constexpr int JEWEL_BANK_DEPALL_W = 120;
    constexpr int JEWEL_BANK_DEPALL_H = 24;
    constexpr int JEWEL_BANK_DEPALL_BOTTOM_GAP = 42; // from window bottom to button top

    // Columns: A = item icon + name, B = loose amount, C = 10-pack amount,
    // D = single action, E = pack action. The D+E header is one mode-toggle button.
    constexpr int JEWEL_BANK_COLUMN_COUNT = 6;
    constexpr int s_JewelBankColumns[JEWEL_BANK_COLUMN_COUNT] =
    {
        0, 196, 266, 336, 389, 442,
    };

    // Headers for columns A, B, C only; D + E are spanned by the mode-toggle button.
    const wchar_t* const s_JewelBankHeaders[3] =
    {
        L"Item",
        L"Amount",
        L"10 Pack",
    };

    const JewelBankItemInfo s_JewelBankItems[CNewUIJewelBank::ITEM_COUNT] =
    {
        { L"Jewel of Bless", ITEM_PACKED_JEWEL_OF_BLESS, 0 },
        { L"Jewel of Soul", ITEM_PACKED_JEWEL_OF_SOUL, 0 },
        { L"Jewel of Life", ITEM_PACKED_JEWEL_OF_LIFE, 0 },
        { L"Jewel of Creation", ITEM_PACKED_JEWEL_OF_CREATION, 0 },
        { L"Jewel of Guardian", ITEM_PACKED_JEWEL_OF_GUARDIAN, 0 },
        { L"Gemstone", ITEM_PACKED_GEMSTONE, 0 },
        { L"Jewel of Harmony", ITEM_PACKED_JEWEL_OF_HARMONY, 0 },
        { L"Jewel of Chaos", ITEM_PACKED_JEWEL_OF_CHAOS, 0 },
        { L"Lower refine stone", ITEM_PACKED_LOWER_REFINE_STONE, 0 },
        { L"Higher refine stone", ITEM_PACKED_HIGHER_REFINE_STONE, 0 },
        { L"Box of Kundun +1", ITEM_BOX_OF_LUCK, 8 },
        { L"Box of Kundun +2", ITEM_BOX_OF_LUCK, 9 },
        { L"Box of Kundun +3", ITEM_BOX_OF_LUCK, 10 },
        { L"Box of Kundun +4", ITEM_BOX_OF_LUCK, 11 },
        { L"Box of Kundun +5", ITEM_BOX_OF_LUCK, 12 },
        { L"Blue Chocolate Box", ITEM_BLUE_CHOCOLATE_BOX, 0 },
        { L"Pink Chocolate Box", ITEM_PINK_CHOCOLATE_BOX, 0 },
    };

    void RenderJewelBankRect(int x, int y, int width, int height, float red, float green, float blue, float alpha)
    {
        // Flat colour fill via the engine RenderColor primitive.
        //
        // Root-cause fix for the "invisible fills" bug seen in-game: EndRenderColor()
        // re-enables texturing with a raw glEnable(GL_TEXTURE_2D) but never updates the
        // engine's TextureEnable bookkeeping flag, and BindTexture()/RenderImage() never
        // touch that flag either. So after the very first RenderImage of the frame (the
        // window-frame slices, the item boxes, the close button, ...) TextureEnable is
        // stale-false. RenderColor's internal DisableTexture() then sees
        // TextureEnable==false and SKIPS the real glDisable(GL_TEXTURE_2D): the untextured
        // quad is drawn with a stale texture still bound and stale UVs, and the global
        // glAlphaFunc(GL_GREATER, 0.25f) discards it. That is exactly why only the body
        // fill (drawn before any RenderImage) survived while the header band, grid
        // dividers and every button bevel vanished. Forcing the texture off here -
        // independent of the stale flag - makes the fill render reliably; EndRenderColor
        // restores texturing for the next RenderImage.
        if (width <= 0 || height <= 0 || alpha <= 0.f)
            return;

        glColor4f(red, green, blue, alpha);
        glDisable(GL_TEXTURE_2D);
        RenderColor(float(x), float(y), float(width), float(height));
        EndRenderColor();
    }

    void RenderJewelBankFrame(int x, int y, int width, int height)
    {
        RenderImage(CNewUIJewelBank::IMAGE_TABLE_TOP_LEFT, x, y, 14.f, 14.f);
        RenderImage(CNewUIJewelBank::IMAGE_TABLE_TOP_RIGHT, x + width - 14.f, y, 14.f, 14.f);
        RenderImage(CNewUIJewelBank::IMAGE_TABLE_BOTTOM_LEFT, x, y + height - 14.f, 14.f, 14.f);
        RenderImage(CNewUIJewelBank::IMAGE_TABLE_BOTTOM_RIGHT, x + width - 14.f, y + height - 14.f, 14.f, 14.f);
        RenderImage(CNewUIJewelBank::IMAGE_TABLE_TOP_PIXEL, x + 6.f, y, width - 12.f, 14.f);
        RenderImage(CNewUIJewelBank::IMAGE_TABLE_BOTTOM_PIXEL, x + 6.f, y + height - 14.f, width - 12.f, 14.f);
        RenderImage(CNewUIJewelBank::IMAGE_TABLE_LEFT_PIXEL, x, y + 6.f, 14.f, height - 12.f);
        RenderImage(CNewUIJewelBank::IMAGE_TABLE_RIGHT_PIXEL, x + width - 14.f, y + 6.f, 14.f, height - 12.f);
    }

    void RenderJewelBankLine(int x, int y, int width, int height, float red, float green, float blue, float alpha)
    {
        RenderJewelBankRect(x, y, width, height, red, green, blue, alpha);
    }

    void RenderJewelBankTexture(int x, int y, int width, int height)
    {
        for (int offsetY = 4; offsetY < height - 4; offsetY += 7)
        {
            const float alpha = ((offsetY / 7) % 2 == 0) ? 0.12f : 0.06f;
            RenderJewelBankRect(x + 3, y + offsetY, width - 6, 1, 0.22f, 0.23f, 0.24f, alpha);
        }

        for (int offsetX = 17; offsetX < width - 6; offsetX += 31)
        {
            RenderJewelBankRect(x + offsetX, y + 3, 1, height - 6, 0.00f, 0.00f, 0.00f, 0.10f);
        }
    }

    // A simple flat button drawn entirely from RenderColor primitives -- NO IGS texture.
    //
    // WHY NOT THE IGS BUTTON: the native IGS art stacks its UP/OVER/DOWN frames vertically
    // and the engine samples them at sv = state * buttonHeight (NewUIButton.cpp Render). When
    // a button is shorter than the texture's native frame (~26px), the OVER/DOWN sample
    // straddles a frame boundary and smears across the quad -- the "broken on hover" artefact
    // on the small +1/+10 buttons. A primitive button has no frames, so it stays crisp at ANY
    // size and gives clean hover/press feedback. The CNewUIButton is still used for hit-testing
    // (UpdateMouseEvent is purely geometric) and tooltips, so clicks and packets are unchanged;
    // only the visual is ours. The label is centred with RT3_SORT_CENTER over the full button
    // width (the same reliable path the table headers/amounts use), so it cannot drift off.
    void RenderJewelBankButton(CNewUIButton& btn, const wchar_t* label, int textR, int textG, int textB)
    {
        const POINT& p = btn.GetPos();
        const POINT& sz = btn.GetSize();
        const int x = p.x, y = p.y, w = sz.x, h = sz.y;
        if (w <= 2 || h <= 2)
            return;

        const BUTTON_STATE st = btn.GetBTState();
        const bool over = (st == BUTTON_STATE_OVER || st == BUTTON_STATE_DOWN);
        const bool down = (st == BUTTON_STATE_DOWN);

        // Dark navy outer edge.
        RenderJewelBankRect(x, y, w, h, 0.020f, 0.030f, 0.050f, 1.0f);

        // Inner face: navy, a touch lighter than the window so the button reads as raised;
        // brighten (steel-blue) on hover, sink on press. Matches the Duel Ladder palette.
        float fr = 0.105f, fg = 0.140f, fb = 0.220f;
        if (down)      { fr = 0.075f; fg = 0.100f; fb = 0.160f; }
        else if (over) { fr = 0.155f; fg = 0.205f; fb = 0.315f; }
        RenderJewelBankRect(x + 1, y + 1, w - 2, h - 2, fr, fg, fb, 1.0f);

        // Bevel: gold top/left + dark bottom/right, inverted while pressed (inset look).
        const float hiR = down ? 0.020f : 0.620f, hiG = down ? 0.030f : 0.520f, hiB = down ? 0.050f : 0.300f;
        const float loR = down ? 0.620f : 0.020f, loG = down ? 0.520f : 0.030f, loB = down ? 0.300f : 0.050f;
        RenderJewelBankRect(x + 1, y + 1, w - 2, 1, hiR, hiG, hiB, 0.80f);        // top
        RenderJewelBankRect(x + 1, y + 1, 1, h - 2, hiR, hiG, hiB, 0.80f);        // left
        RenderJewelBankRect(x + 1, y + h - 2, w - 2, 1, loR, loG, loB, 0.80f);    // bottom
        RenderJewelBankRect(x + w - 2, y + 1, 1, h - 2, loR, loG, loB, 0.80f);    // right

        if (label && label[0])
        {
            // Vertical centring matches the table's row-text baseline: RenderTable centres a row
            // label at rowY+4 in a 17px row, i.e. an effective glyph height of ~9px, so the
            // centred top is y + (h - 9)/2. Horizontal centring is RT3_SORT_CENTER over the full
            // button width -- the same reliable path the headers/amounts use -- so it never drifts.
            g_pRenderText->SetFont(g_hFont);
            g_pRenderText->SetBgColor(0);
            g_pRenderText->SetTextColor(textR, textG, textB, 255);
            g_pRenderText->RenderText(x, y + (h - 9) / 2, label, w, 0, RT3_SORT_CENTER);
        }
    }
}

CNewUIJewelBank::CNewUIJewelBank()
{
    m_pNewUIMng = NULL;
    m_pNewUI3DRenderMng = NULL;
    m_Pos.x = 0;
    m_Pos.y = 0;
    m_DepositMode = true;
    for (int i = 0; i < ITEM_COUNT; i++)
        m_Balances[i] = 0;
}

CNewUIJewelBank::~CNewUIJewelBank()
{
    Release();
}

bool CNewUIJewelBank::Create(CNewUIManager* pNewUIMng, CNewUI3DRenderMng* pNewUI3DRenderMng, int x, int y)
{
    if (NULL == pNewUIMng || NULL == pNewUI3DRenderMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(INTERFACE_JEWELBANK, this);

    m_pNewUI3DRenderMng = pNewUI3DRenderMng;
    m_pNewUI3DRenderMng->Add3DRenderObj(this, INFORMATION_CAMERA_Z_ORDER);

    SetPos((640 - WINDOW_WIDTH) / 2, 14);
    LoadImages();
    InitButtons();
    Show(false);

    return true;
}

void CNewUIJewelBank::Release()
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

void CNewUIJewelBank::SetPos(int x, int y)
{
    m_Pos.x = x;
    m_Pos.y = y;
}

void CNewUIJewelBank::LoadImages()
{
    LoadBitmap(L"Interface\\newui_msgbox_back.jpg", IMAGE_BASE_WINDOW_BACK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_box.tga", IMAGE_ITEM_BOX, GL_LINEAR);
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

void CNewUIJewelBank::UnloadImages()
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
    DeleteBitmap(IMAGE_ITEM_BOX);
    DeleteBitmap(IMAGE_BASE_WINDOW_BACK);
}

void CNewUIJewelBank::InitButtons()
{
    const int tableX = m_Pos.x + JEWEL_BANK_TABLE_X;

    // The action / toggle / Deposit-All buttons are drawn as simple flat primitive buttons
    // (RenderJewelBankButton), NOT the IGS texture. The IGS art stacks its UP/OVER/DOWN frames
    // vertically and samples them at state*height, so at these small button heights the hover
    // frame straddled a frame boundary and smeared ("broken on hover"). A primitive button has
    // no frames -> crisp at any size + reliable RT3_SORT_CENTER labels. Here we only register
    // each button's HIT-TEST geometry (UpdateMouseEvent is purely geometric) and its tooltip;
    // the label is left empty (ChangeText L"") because RenderJewelBankButton draws it. No IGS
    // image is registered on these buttons. Only the close button still uses native texture art.
    auto setupActionButton = [&](CNewUIButton& button, int column, int rowY, const wchar_t* tooltip)
    {
        const int columnX = tableX + s_JewelBankColumns[column];
        const int columnWidth = s_JewelBankColumns[column + 1] - s_JewelBankColumns[column];
        const int buttonX = columnX + ((columnWidth - JEWEL_BANK_ACTION_W) / 2);
        const int buttonY = rowY + ((ROW_HEIGHT - JEWEL_BANK_ACTION_H) / 2);

        button.ChangeButtonInfo(buttonX, buttonY, JEWEL_BANK_ACTION_W, JEWEL_BANK_ACTION_H);
        button.ChangeText(L"");
        button.ChangeToolTipText(tooltip, TRUE);
    };

    for (int i = 0; i < ITEM_COUNT; i++)
    {
        const int rowY = m_Pos.y + ROW_START_Y + i * ROW_HEIGHT;
        setupActionButton(m_BtnActionSingle[i], 3, rowY, L"Deposit / withdraw one");
        setupActionButton(m_BtnActionPack[i], 4, rowY, L"Deposit / withdraw a 10-pack");
    }

    // Mode toggle: a flat button centred over the D + E header span, switching Deposit <->
    // Withdraw. RenderJewelBankButton draws it; the DEPOSIT/WITHDRAW caption is set each frame
    // in Render(). Hit-test geometry + tooltip only here.
    const int toggleCenterX = tableX + (s_JewelBankColumns[3] + s_JewelBankColumns[5]) / 2;
    const int toggleX = toggleCenterX - JEWEL_BANK_TOGGLE_W / 2;
    const int toggleY = m_Pos.y + JEWEL_BANK_TABLE_Y + (JEWEL_BANK_HEADER_HEIGHT - JEWEL_BANK_TOGGLE_H) / 2;
    m_BtnModeToggle.ChangeButtonInfo(toggleX, toggleY, JEWEL_BANK_TOGGLE_W, JEWEL_BANK_TOGGLE_H);
    m_BtnModeToggle.ChangeText(L"");
    m_BtnModeToggle.ChangeToolTipText(L"Switch between Deposit and Withdraw", TRUE);

    // Deposit-all: flat button centred along the bottom, above the in-game skill bar.
    m_BtnDepositAll.ChangeButtonInfo(m_Pos.x + (WINDOW_WIDTH - JEWEL_BANK_DEPALL_W) / 2, m_Pos.y + WINDOW_HEIGHT - JEWEL_BANK_DEPALL_BOTTOM_GAP, JEWEL_BANK_DEPALL_W, JEWEL_BANK_DEPALL_H);
    m_BtnDepositAll.ChangeText(L"");
    m_BtnDepositAll.ChangeToolTipText(L"Deposit every jewel, pack and box from your inventory", TRUE);

    // Close button: also a flat warm button (matching the action/toggle/Deposit-All buttons)
    // with an "X" glyph, instead of the ornate native IGS X texture -- per user request to make
    // it match the rest of the menu. Geometry + tooltip only here; RenderJewelBankButton draws
    // it. Aligned neatly inside the title band at the top-right. IMAGE_BASE_WINDOW_BTN_EXIT is
    // no longer used (still loaded/freed, harmless).
    m_BtnClose.ChangeButtonInfo(m_Pos.x + WINDOW_WIDTH - 42, m_Pos.y + 7, 30, 22);
    m_BtnClose.ChangeText(L"");
    m_BtnClose.ChangeToolTipText(L"Close", TRUE);
}

float CNewUIJewelBank::GetLayerDepth()
{
    return 3.5f;
}

float CNewUIJewelBank::GetKeyEventOrder()
{
    return 3.5f;
}

void CNewUIJewelBank::SetBalances(const unsigned int* pBalances)
{
    if (pBalances == NULL)
        return;

    for (int i = 0; i < ITEM_COUNT; i++)
        m_Balances[i] = pBalances[i];
}

void CNewUIJewelBank::SendRequest(BYTE op, BYTE arg1, WORD arg2, WORD arg3)
{
    if (SocketClient == NULL)
        return;

    SocketClient->ToGameServer()->SendJewelBankRequest(op, arg1, arg2, arg3);
}

void CNewUIJewelBank::Toggle()
{
    if (IsVisible())
    {
        Show(false);
        return;
    }

    Show(true);
    SendRequest(0, 0, 0, 0); // query current balances
}

bool CNewUIJewelBank::Update()
{
    if (IsVisible())
    {
        const BYTE op = m_DepositMode ? 1 : 2; // 1 = deposit, 2 = withdraw

        for (int i = 0; i < ITEM_COUNT; i++)
        {
            if (m_BtnActionSingle[i].UpdateMouseEvent())
                SendRequest(op, (BYTE)i, 0, 0);

            if (m_BtnActionPack[i].UpdateMouseEvent())
                SendRequest(op, (BYTE)i, 1, 0);
        }

        if (m_BtnModeToggle.UpdateMouseEvent())
            m_DepositMode = !m_DepositMode;

        if (m_BtnDepositAll.UpdateMouseEvent())
            SendRequest(3, 0, 0, 0); // deposit all

        if (m_BtnClose.UpdateMouseEvent())
            g_pNewUISystem->Hide(INTERFACE_JEWELBANK);
    }
    return true;
}

bool CNewUIJewelBank::UpdateMouseEvent()
{
    if (!CheckMouseIn(m_Pos.x, m_Pos.y, WINDOW_WIDTH, WINDOW_HEIGHT))
        return true;

    return false;
}

bool CNewUIJewelBank::UpdateKeyEvent()
{
    if (IsVisible())
    {
        if (IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(INTERFACE_JEWELBANK);
            return false;
        }
    }
    return true;
}

bool CNewUIJewelBank::IsVisible() const
{
    return CNewUIObj::IsVisible();
}

void CNewUIJewelBank::RenderBack()
{
    // Window background.
    //
    // The stock msgbox texture (newui_msgbox_back.jpg) is a NARROW 190x429 vertical panel;
    // every shipped window draws it at exactly 190x429. Stretching it across this 470-wide
    // window is fundamentally wrong: the 4-arg path overflows UV and GL_CLAMP smears the dark
    // right edge (the original "half black"), and even a full-UV stretch smears the texture's
    // dark side bevels into residual black. So we DON'T stretch a narrow texture here.
    //
    // Instead the interior is a flat, fully-opaque dark gothic fill via the engine RenderColor
    // primitive (RenderJewelBankRect, alpha 1.0) -- guaranteed no black, ever -- and the native
    // item-table border slices are drawn on top as the 9-slice frame. The header band, grid
    // dividers, row stripes, item icons and gold frame supply the visual richness.
    RenderJewelBankRect(m_Pos.x, m_Pos.y, WINDOW_WIDTH, WINDOW_HEIGHT, 0.055f, 0.075f, 0.130f, 1.0f);
    RenderJewelBankFrame(m_Pos.x, m_Pos.y, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Title band along the top (inset darker so the gold title reads).
    RenderJewelBankRect(m_Pos.x + 8, m_Pos.y + 7, WINDOW_WIDTH - 16, 24, 0.035f, 0.050f, 0.092f, 1.0f);
    RenderJewelBankRect(m_Pos.x + 8, m_Pos.y + 31, WINDOW_WIDTH - 16, 1, 0.42f, 0.34f, 0.20f, 0.85f);

    g_pRenderText->SetFont(g_hFontBold);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(236, 206, 120, 255);
    g_pRenderText->RenderText(m_Pos.x, m_Pos.y + 12, L"JEWEL BANK", WINDOW_WIDTH, 0, RT3_SORT_CENTER);
}

void CNewUIJewelBank::RenderTable()
{
    const int tableX = m_Pos.x + JEWEL_BANK_TABLE_X;
    const int tableY = m_Pos.y + JEWEL_BANK_TABLE_Y;

    // Header band (columns A/B/C text; D+E are covered by the mode-toggle button).
    RenderJewelBankRect(tableX, tableY, JEWEL_BANK_TABLE_WIDTH, JEWEL_BANK_HEADER_HEIGHT, 0.070f, 0.095f, 0.150f, 0.95f);
    RenderJewelBankRect(tableX, tableY + JEWEL_BANK_HEADER_HEIGHT - 1, JEWEL_BANK_TABLE_WIDTH, 1, 0.42f, 0.34f, 0.20f, 0.85f);

    g_pRenderText->SetFont(g_hFontBold);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(228, 206, 150, 255);
    g_pRenderText->RenderText(tableX + 33, tableY + 5, s_JewelBankHeaders[0], s_JewelBankColumns[1] - 35, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(tableX + s_JewelBankColumns[1], tableY + 5, s_JewelBankHeaders[1], s_JewelBankColumns[2] - s_JewelBankColumns[1], 0, RT3_SORT_CENTER);
    g_pRenderText->RenderText(tableX + s_JewelBankColumns[2], tableY + 5, s_JewelBankHeaders[2], s_JewelBankColumns[3] - s_JewelBankColumns[2], 0, RT3_SORT_CENTER);

    // Grid: vertical column dividers. A|B, B|C, C|D run through the header and rows;
    // D|E runs through the rows only (the mode-toggle button covers the D+E header cell).
    const int rowsTop = m_Pos.y + ROW_START_Y;
    const int bodyBottom = m_Pos.y + ROW_START_Y + ITEM_COUNT * ROW_HEIGHT;
    for (int c = 1; c <= 3; c++)
        RenderJewelBankRect(tableX + s_JewelBankColumns[c], tableY, 1, bodyBottom - tableY, 0.42f, 0.34f, 0.20f, 0.30f);
    RenderJewelBankRect(tableX + s_JewelBankColumns[4], rowsTop, 1, bodyBottom - rowsTop, 0.42f, 0.34f, 0.20f, 0.30f);
    RenderJewelBankRect(tableX, bodyBottom, JEWEL_BANK_TABLE_WIDTH, 1, 0.42f, 0.34f, 0.20f, 0.45f);

    for (int i = 0; i < ITEM_COUNT; i++)
    {
        const int rowY = m_Pos.y + ROW_START_Y + i * ROW_HEIGHT;
        const JewelBankItemInfo& item = s_JewelBankItems[i];

        if (i % 2 == 1)
            RenderJewelBankRect(tableX, rowY, JEWEL_BANK_TABLE_WIDTH, ROW_HEIGHT, 1.0f, 1.0f, 1.0f, 0.045f);

        RenderImage(IMAGE_ITEM_BOX, tableX + 8, rowY + 1, 20.f, 18.f);

        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(238, 199, 86, 255);
        g_pRenderText->RenderText(tableX + 33, rowY + 4, item.Name, s_JewelBankColumns[1] - 35, 0, RT3_SORT_LEFT);

        unsigned int bal = m_Balances[i];
        wchar_t amount[32];
        wchar_t packAmount[32];
        std::swprintf(amount, 32, L"%u", bal % 10);
        std::swprintf(packAmount, 32, L"%u", bal / 10);
        g_pRenderText->SetTextColor(238, 196, 105, 255);
        g_pRenderText->RenderText(tableX + s_JewelBankColumns[1], rowY + 4, amount, s_JewelBankColumns[2] - s_JewelBankColumns[1], 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(tableX + s_JewelBankColumns[2], rowY + 4, packAmount, s_JewelBankColumns[3] - s_JewelBankColumns[2], 0, RT3_SORT_CENTER);
    }
}

void CNewUIJewelBank::Render3D()
{
    if (!IsVisible())
        return;

    const int tableX = m_Pos.x + JEWEL_BANK_TABLE_X;

    for (int i = 0; i < ITEM_COUNT; i++)
    {
        const int rowY = m_Pos.y + ROW_START_Y + i * ROW_HEIGHT;
        const JewelBankItemInfo& item = s_JewelBankItems[i];

        glColor4f(1.f, 1.f, 1.f, 1.f);
        RenderItem3D(float(tableX + 6), float(rowY - 1), 24.f, 20.f, item.Type, item.Level, 0, 0, false);
    }
}

bool CNewUIJewelBank::Render()
{
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, 1.f);

    RenderBack();
    RenderTable();

    // The action / toggle / Deposit-All buttons are flat primitive buttons (RenderJewelBankButton):
    // the IGS texture smeared on hover at these small button sizes (its UP/OVER/DOWN frames are
    // stacked vertically and sampled at state*height). We draw the face + a RT3_SORT_CENTER label
    // from each button's live hover/press state, then call the button's own Render() which -- with
    // no image and no name registered -- draws ONLY the tooltip. Clicks are handled in Update()
    // via UpdateMouseEvent (pure geometry), so packets/behaviour are unchanged.
    //
    // The +1/+10/-1/-10 and DEPOSIT/WITHDRAW captions flip with the deposit/withdraw mode.
    const wchar_t* singleLabel = m_DepositMode ? L"+1" : L"-1";
    const wchar_t* packLabel = m_DepositMode ? L"+10" : L"-10";
    for (int i = 0; i < ITEM_COUNT; i++)
    {
        RenderJewelBankButton(m_BtnActionSingle[i], singleLabel, 255, 236, 188);
        m_BtnActionSingle[i].Render();

        RenderJewelBankButton(m_BtnActionPack[i], packLabel, 255, 236, 188);
        m_BtnActionPack[i].Render();
    }

    RenderJewelBankButton(m_BtnModeToggle, m_DepositMode ? L"DEPOSIT" : L"WITHDRAW", 255, 238, 200);
    m_BtnModeToggle.Render();

    RenderJewelBankButton(m_BtnDepositAll, L"DEPOSIT ALL", 255, 238, 200);
    m_BtnDepositAll.Render();

    // Close button: same flat warm style as the rest; the "X" is a warm amber so it still reads
    // as a close affordance while matching the palette. m_BtnClose.Render() adds only the tooltip.
    RenderJewelBankButton(m_BtnClose, L"X", 240, 170, 110);
    m_BtnClose.Render();

    DisableAlphaBlend();

    return true;
}
