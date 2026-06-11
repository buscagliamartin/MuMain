#include "stdafx.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <cwctype>

#include "UI/NewUI/DuelLadderWindow.h"
#include "UI/NewUI/NewUICommon.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Dialogs/NewUIMessageBox.h"
#include "UI/Legacy/UIControls.h"   // CUITextInputBox (SEARCH PLAYER field)
#include "Network/Server/WSclient.h"
#include "Render/Sprites/GlobalBitmap.h"

using namespace SEASON3B;

namespace
{
    // ---- Large hub layout (local coords, added to m_Pos) -------------------------------
    constexpr int kMargin = 12;

    constexpr int kTitleH = 46;   // a touch taller so the enlarged title sits clear of both rules
    constexpr int kTabY = 50;
    constexpr int kTabH = 28;
    constexpr int kTabGap = 6;

    constexpr int kContentY = 80;
    constexpr int kFiltersBottom = 386;     // filters is a FULL-height left column
    constexpr int kColBottom = 290;         // center + right columns bottom (above the bottom strip)
    constexpr int kBottomY = 296;           // top of the my-status / waiting strip
    constexpr int kBottomBottom = 386;

    // Left filters column (full height) + center/right columns.
    constexpr int kFiltersX = 12;
    constexpr int kFiltersW = 166;
    constexpr int kCenterX = 184;
    constexpr int kCenterW = 250;
    constexpr int kDetailsX = 440;
    constexpr int kDetailsW = 166;

    // Bottom strip spans the center + right columns only (the filters column runs full height).
    constexpr int kStatusX = 184;
    constexpr int kStatusW = 208;
    constexpr int kWaitX = 398;
    constexpr int kWaitW = 208;

    // Shared filter-row Y offsets (from m_Pos.y) so InitButtons hitboxes and the rendered
    // labels/boxes always line up. Labels sit ~12px above their control row.
    constexpr int kfSeasonY  = 106;
    constexpr int kfBracketY = 134;
    constexpr int kfClassY   = 162;
    constexpr int kfTierY    = 190;   // tier row 2 at +20
    constexpr int kfRatingY  = 238;
    constexpr int kfWinRateY = 264;
    constexpr int kfSearchY  = 292;
    constexpr int kfGuildY   = 320;
    constexpr int kfBtnsY    = 348;

    const wchar_t* const s_TabNames[CNewUIDuelLadder::TAB_COUNT] =
    {
        L"RANKINGS", L"MY PROFILE", L"MATCH HISTORY", L"SEASON REWARDS", L"HALL OF FAME",
    };

    const wchar_t* const s_DuelTierNames[6] =
    {
        L"Bronze", L"Silver", L"Gold", L"Platinum", L"Diamond", L"Master",
    };
    const wchar_t* const s_BracketNames[3] = { L"1v1", L"Best of 3", L"Best of 5" };
    const wchar_t* const s_ClassNames[7] = { L"DK", L"DW", L"ELF", L"SUM", L"MG", L"DL", L"RF" };
    const wchar_t* const s_SortNames[4] = { L"Rating", L"Wins", L"Win Rate", L"Rank" };

    const wchar_t* GetTierName(BYTE tier)
    {
        return tier < 6 ? s_DuelTierNames[tier] : L"-";
    }

    const wchar_t* GetClassName(BYTE classNumber)
    {
        // Server packs OpenMU CharacterClass.Number; base group = Number >> 2.
        switch (classNumber >> 2)
        {
        case 0: return L"DW";
        case 1: return L"DK";
        case 2: return L"ELF";
        case 3: return L"MG";
        case 4: return L"DL";
        case 5: return L"SUM";
        case 6: return L"RF";
        default: return L"?";
        }
    }

    unsigned int GetWinRate(unsigned int wins, unsigned int losses)
    {
        const unsigned int total = wins + losses;
        return total == 0 ? 0 : (wins * 100) / total;
    }

    unsigned int ReadUInt32(const BYTE* data)
    {
        unsigned int value = 0;
        std::memcpy(&value, data, sizeof(value));
        return value;
    }

    void ConvertNameToWide(const char* name, wchar_t* out, int outCount)
    {
        if (outCount <= 0) return;
        if (name == NULL) { out[0] = 0; return; }
        int i = 0;
        for (; i < outCount - 1 && name[i]; ++i)
            out[i] = (wchar_t)(unsigned char)name[i];
        out[i] = 0;
    }

    // Overload for already-wide source names (e.g. Hero->ID is wchar_t[]).
    void ConvertNameToWide(const wchar_t* name, wchar_t* out, int outCount)
    {
        if (outCount <= 0) return;
        if (name == NULL) { out[0] = 0; return; }
        int i = 0;
        for (; i < outCount - 1 && name[i]; ++i)
            out[i] = name[i];
        out[i] = 0;
    }

    // Flat colour fill via the engine's proven RenderColor primitive (applies ConvertX/Y + Y
    // flip itself; Alpha==0 keeps our glColor4f; EndRenderColor restores state).
    void RenderUiRect(int x, int y, int width, int height, float red, float green, float blue, float alpha)
    {
        if (width <= 0 || height <= 0 || alpha <= 0.f) return;
        glColor4f(red, green, blue, alpha);
        // CRITICAL (same fix as the Jewel Bank's RenderJewelBankRect): force the texture OFF.
        // EndRenderColor() re-enables GL_TEXTURE_2D via a raw glEnable without updating the
        // engine's TextureEnable flag, so after the first RenderImage of the frame (the gold
        // 9-slice border) every subsequent RenderColor fill is drawn textured-with-stale-UVs and
        // discarded by glAlphaFunc(GL_GREATER, 0.25f) -- which is why only the window-body fill
        // (drawn before any RenderImage) survived and all cards/buttons/tabs looked like flat
        // floating text. Forcing glDisable here makes every fill render.
        glDisable(GL_TEXTURE_2D);
        RenderColor(float(x), float(y), float(width), float(height));
        EndRenderColor();
    }

    void RenderTextEx(int x, int y, int width, const wchar_t* text, BYTE red, BYTE green, BYTE blue, BYTE alpha = 255, int align = RT3_SORT_LEFT)
    {
        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(red, green, blue, alpha);
        g_pRenderText->RenderText(x, y, text, width, 0, align);
    }

    void RenderLabelValue(int x, int y, int width, const wchar_t* label, const wchar_t* value, BYTE vr, BYTE vg, BYTE vb)
    {
        RenderTextEx(x, y, width / 2, label, 196, 178, 142, 255, RT3_SORT_LEFT);
        RenderTextEx(x + width / 2, y, width / 2, value, vr, vg, vb, 255, RT3_SORT_RIGHT);
    }
}

CNewUIDuelLadder::CNewUIDuelLadder()
{
    m_pNewUIMng = NULL;
    m_Pos.x = m_Pos.y = 0;
    m_CurrentTab = TAB_RANKINGS;
    m_CurrentBracket = 1;
    m_HofBracket = 1;
    m_Page = 0;
    m_SortMode = 0;
    m_EntryCount = 0;
    std::memset(m_Entries, 0, sizeof(m_Entries));

    m_ProfileBracket = 0;
    m_ProfileTier = 0;
    m_ProfileRating = 0;
    m_ProfileWins = 0;
    m_ProfileLosses = 0;
    m_ProfileRank = 0;

    m_HoveredRow = -1;
    m_SelectedRow = -1;
    m_ClassFilter = 0;
    m_TierFilter = 0;
    m_RatingMinSel = 0;
    m_WinRateMinSel = 0;
    m_ActiveSeason = 0;
    m_WaitingScroll = 0;
    m_pSearchInput = nullptr;
    m_SearchInputShown = false;
    m_SearchText[0] = 0;
    m_pGuildInput = nullptr;
    m_GuildInputShown = false;
    m_GuildText[0] = 0;
    m_hTitleFont = NULL;

    m_IsWaiting = false;
    m_WaitingCount = 0;
    std::memset(m_Waiting, 0, sizeof(m_Waiting));
    m_HistoryCount = 0;
    std::memset(m_History, 0, sizeof(m_History));
    m_HofCount = 0;
    std::memset(m_Hof, 0, sizeof(m_Hof));
}

CNewUIDuelLadder::~CNewUIDuelLadder()
{
    Release();
}

bool CNewUIDuelLadder::Create(CNewUIManager* pNewUIMng, int x, int y)
{
    if (pNewUIMng == NULL) return false;
    m_pNewUIMng = pNewUIMng;

    LoadImages();
    SetPos(x, y);
    InitButtons();

    m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_DUELLADDER, this);
    return true;
}

void CNewUIDuelLadder::Release()
{
    UnloadImages();
    if (m_pSearchInput != nullptr)
    {
        delete m_pSearchInput;
        m_pSearchInput = nullptr;
    }
    if (m_pGuildInput != nullptr)
    {
        delete m_pGuildInput;
        m_pGuildInput = nullptr;
    }
    if (m_hTitleFont != NULL)
    {
        DeleteObject(m_hTitleFont);
        m_hTitleFont = NULL;
    }
}

// Build a larger bold title font by scaling the stock UI font up ~70% (resolution-aware: it copies
// the live LOGFONT of g_hFontBold, so it tracks the same DPI). No new asset — same Tahoma face.
void CNewUIDuelLadder::EnsureTitleFont()
{
    if (m_hTitleFont != NULL) return;
    LOGFONTW lf;
    std::memset(&lf, 0, sizeof(lf));
    if (GetObjectW(g_hFontBold, sizeof(lf), &lf) == 0)
        return;
    lf.lfHeight = (LONG)(lf.lfHeight * 1.7);
    lf.lfWidth = 0;
    lf.lfWeight = FW_BOLD;
    m_hTitleFont = CreateFontIndirectW(&lf);
}

// ---- SEARCH PLAYER text field ------------------------------------------------------------
// A native Win32 edit control. Because focus moves to the edit window, the game's key-state
// table (fed only from g_hWnd's message stream) doesn't see the typed keys, so movement/skill
// hotkeys stay dormant while typing — the same reason the login/chat boxes are safe.

void CNewUIDuelLadder::EnsureSearchInput()
{
    if (m_pSearchInput != nullptr || g_hWnd == NULL) return;
    m_pSearchInput = new CUITextInputBox;
    m_pSearchInput->Init(g_hWnd, 112, 15, NAME_LEN);
    m_pSearchInput->SetBackColor(0, 0, 0, 0);   // transparent: our own field box shows behind it
    m_pSearchInput->SetTextColor(255, 244, 224, 196);
    m_pSearchInput->SetFont(g_hFont);
    m_pSearchInput->SetTextLimit(NAME_LEN);
    m_pSearchInput->SetState(UISTATE_HIDE);
    m_SearchInputShown = false;
}

void CNewUIDuelLadder::UpdateSearchInput()
{
    EnsureSearchInput();
    if (m_pSearchInput == nullptr) return;

    // Position over the SEARCH PLAYER row of the filters panel (reference coords, same space the
    // panel is drawn in; CUITextInputBox scales internally by g_fScreenRate).
    const int bx = m_Pos.x + kFiltersX + 8 + 2;
    const int by = m_Pos.y + kfSearchY + 2;
    m_pSearchInput->SetPosition(bx, by);
    if (!m_SearchInputShown)
    {
        m_pSearchInput->SetState(UISTATE_NORMAL);
        m_SearchInputShown = true;
    }

    // Cache the current text for the filter (trimmed to NAME_LEN).
    m_pSearchInput->GetText(m_SearchText, NAME_LEN + 1);
    m_SearchText[NAME_LEN] = 0;
    if (m_SearchText[0] != 0) m_Page = 0; // a filtered view always starts at the first page
}

void CNewUIDuelLadder::HideSearchInput()
{
    if (m_pSearchInput == nullptr || !m_SearchInputShown) return;
    m_pSearchInput->GetText(m_SearchText, NAME_LEN + 1);
    m_SearchText[NAME_LEN] = 0;
    m_pSearchInput->SetState(UISTATE_HIDE);
    m_SearchInputShown = false;
    if (GetFocus() == m_pSearchInput->GetHandle())
        SetFocus(g_hWnd);
}

// GUILD text field — same native-edit pattern as the search field, positioned on the GUILD row.
void CNewUIDuelLadder::EnsureGuildInput()
{
    if (m_pGuildInput != nullptr || g_hWnd == NULL) return;
    m_pGuildInput = new CUITextInputBox;
    m_pGuildInput->Init(g_hWnd, 112, 15, NAME_LEN);
    m_pGuildInput->SetBackColor(0, 0, 0, 0);
    m_pGuildInput->SetTextColor(255, 244, 224, 196);
    m_pGuildInput->SetFont(g_hFont);
    m_pGuildInput->SetTextLimit(NAME_LEN);
    m_pGuildInput->SetState(UISTATE_HIDE);
    m_GuildInputShown = false;
}

void CNewUIDuelLadder::UpdateGuildInput()
{
    EnsureGuildInput();
    if (m_pGuildInput == nullptr) return;
    const int bx = m_Pos.x + kFiltersX + 8 + 2;
    const int by = m_Pos.y + kfGuildY + 2;
    m_pGuildInput->SetPosition(bx, by);
    if (!m_GuildInputShown)
    {
        m_pGuildInput->SetState(UISTATE_NORMAL);
        m_GuildInputShown = true;
    }
    m_pGuildInput->GetText(m_GuildText, NAME_LEN + 1);
    m_GuildText[NAME_LEN] = 0;
    if (m_GuildText[0] != 0) m_Page = 0;
}

void CNewUIDuelLadder::HideGuildInput()
{
    if (m_pGuildInput == nullptr || !m_GuildInputShown) return;
    m_pGuildInput->GetText(m_GuildText, NAME_LEN + 1);
    m_GuildText[NAME_LEN] = 0;
    m_pGuildInput->SetState(UISTATE_HIDE);
    m_GuildInputShown = false;
    if (GetFocus() == m_pGuildInput->GetHandle())
        SetFocus(g_hWnd);
}

void CNewUIDuelLadder::SetPos(int x, int y)
{
    m_Pos.x = x;
    m_Pos.y = y;
}

void CNewUIDuelLadder::LoadImages()
{
    LoadBitmap(L"Interface\\newui_item_table01(L).tga", IMAGE_DUEL_FRAME_TL, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table01(R).tga", IMAGE_DUEL_FRAME_TR, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table02(L).tga", IMAGE_DUEL_FRAME_BL, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table02(R).tga", IMAGE_DUEL_FRAME_BR, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table03(Up).tga", IMAGE_DUEL_FRAME_TOP, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table03(Dw).tga", IMAGE_DUEL_FRAME_BOTTOM, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table03(L).tga", IMAGE_DUEL_FRAME_LEFT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_table03(R).tga", IMAGE_DUEL_FRAME_RIGHT, GL_LINEAR);
}

void CNewUIDuelLadder::UnloadImages()
{
    DeleteBitmap(IMAGE_DUEL_FRAME_TL);
    DeleteBitmap(IMAGE_DUEL_FRAME_TR);
    DeleteBitmap(IMAGE_DUEL_FRAME_BL);
    DeleteBitmap(IMAGE_DUEL_FRAME_BR);
    DeleteBitmap(IMAGE_DUEL_FRAME_TOP);
    DeleteBitmap(IMAGE_DUEL_FRAME_BOTTOM);
    DeleteBitmap(IMAGE_DUEL_FRAME_LEFT);
    DeleteBitmap(IMAGE_DUEL_FRAME_RIGHT);
}

void CNewUIDuelLadder::InitButtons()
{
    const int x = m_Pos.x;
    const int y = m_Pos.y;

    // Window controls.
    m_BtnClose.ChangeButtonInfo(x + WINDOW_WIDTH - 34, y + 10, 24, 22);
    m_BtnClose.ChangeToolTipText(L"Close", TRUE);

    // Tab bar.
    const int tabW = (WINDOW_WIDTH - 2 * kMargin - (TAB_COUNT - 1) * kTabGap) / TAB_COUNT;
    for (int i = 0; i < TAB_COUNT; ++i)
    {
        m_BtnTab[i].ChangeButtonInfo(x + kMargin + i * (tabW + kTabGap), y + kTabY, tabW, kTabH);
        m_BtnTab[i].ChangeToolTipText(s_TabNames[i], TRUE);
    }

    // ---- Filter controls (full-height left column). Positions use the shared kf*Y offsets
    //      so the rendered labels/boxes line up exactly with these hitboxes. ----
    const int fx = x + kFiltersX + 8;
    const int fw = kFiltersW - 16;            // 150
    const int third = fw / 3;                 // 50
    m_BtnSeason.ChangeButtonInfo(fx, y + kfSeasonY, fw, 18);
    m_BtnSeason.ChangeToolTipText(L"Season (pending server support)", TRUE);
    m_BtnBracket[0].ChangeButtonInfo(fx, y + kfBracketY, 72, 18); // only 1v1 is configured
    for (int i = 0; i < 7; ++i)
        m_BtnClass[i].ChangeButtonInfo(fx + i * (fw / 7), y + kfClassY, fw / 7 - 2, 16);
    for (int i = 0; i < 6; ++i)
        m_BtnTier[i].ChangeButtonInfo(fx + (i % 3) * third, y + kfTierY + (i / 3) * 20, third - 3, 16);
    for (int i = 0; i < 5; ++i)
        m_BtnRating[i].ChangeButtonInfo(fx + i * (fw / 5), y + kfRatingY, fw / 5 - 2, 16);
    for (int i = 0; i < 5; ++i)
        m_BtnWinRate[i].ChangeButtonInfo(fx + i * (fw / 5), y + kfWinRateY, fw / 5 - 2, 16);
    m_BtnSearchPlayer.ChangeButtonInfo(fx + fw - 30, y + kfSearchY, 30, 18);
    m_BtnSearchPlayer.ChangeToolTipText(L"Search player (pending server support)", TRUE);
    m_BtnSearchGuild.ChangeButtonInfo(fx + fw - 30, y + kfGuildY, 30, 18);
    m_BtnSearchGuild.ChangeToolTipText(L"Search guild (pending server support)", TRUE);
    m_BtnResetFilters.ChangeButtonInfo(fx, y + kfBtnsY, fw / 2 - 3, 22);
    m_BtnApplyFilters.ChangeButtonInfo(fx + fw / 2 + 3, y + kfBtnsY, fw / 2 - 3, 22);

    // ---- Pagination + sort (center column) ----
    const int cx = x + kCenterX + 8;
    const int py = y + kColBottom - 26;
    m_BtnFirst.ChangeButtonInfo(cx, py, 40, 22);
    m_BtnPrev.ChangeButtonInfo(cx + 44, py, 40, 22);
    m_BtnNext.ChangeButtonInfo(cx + 120, py, 40, 22);
    m_BtnLast.ChangeButtonInfo(cx + 164, py, 40, 22);
    m_BtnSort.ChangeButtonInfo(x + kCenterX + kCenterW - 80, y + kContentY + 6, 72, 18);
    m_BtnSort.ChangeToolTipText(L"Sort (pending server support)", TRUE);

    // ---- Player-details actions (right column) ----
    const int dx = x + kDetailsX + 8;
    const int dw = kDetailsW - 16;
    m_BtnChallenge.ChangeButtonInfo(dx, y + kColBottom - 62, dw, 26);
    m_BtnChallenge.ChangeToolTipText(L"Send a duel challenge to the selected player", TRUE);
    m_BtnViewGear.ChangeButtonInfo(dx, y + kColBottom - 30, dw / 2 - 3, 22);
    m_BtnViewGear.ChangeToolTipText(L"View gear (pending server support)", TRUE);
    m_BtnCompare.ChangeButtonInfo(dx + dw / 2 + 3, y + kColBottom - 30, dw / 2 - 3, 22);
    m_BtnCompare.ChangeToolTipText(L"Compare (pending server support)", TRUE);

    // ---- Waiting-to-fight (bottom-right) ----
    const int wx = x + kWaitX + 8;
    const int ww = kWaitW - 16;
    m_BtnListMe.ChangeButtonInfo(wx, y + kBottomY + 22, 70, 20);
    m_BtnListMe.ChangeToolTipText(L"List yourself as available for challenges", TRUE);
    m_BtnRemoveMe.ChangeButtonInfo(wx + 74, y + kBottomY + 22, 56, 20);
    m_BtnRemoveMe.ChangeToolTipText(L"Remove yourself from the waiting list", TRUE);
    m_BtnRefreshWaiting.ChangeButtonInfo(wx + 134, y + kBottomY + 22, ww - 134, 20);
    m_BtnRefreshWaiting.ChangeToolTipText(L"Refresh the waiting list", TRUE);
    m_BtnWaitUp.ChangeButtonInfo(wx + ww - 36, y + kBottomY + 5, 17, 13);
    m_BtnWaitUp.ChangeToolTipText(L"Scroll up", TRUE);
    m_BtnWaitDown.ChangeButtonInfo(wx + ww - 18, y + kBottomY + 5, 17, 13);
    m_BtnWaitDown.ChangeToolTipText(L"Scroll down", TRUE);
    for (int i = 0; i < WAITING_VISIBLE; ++i)
        m_BtnWaitingChallenge[i].ChangeButtonInfo(wx + ww - 52, y + kBottomY + 48 + i * 14, 52, 13);

    // ---- Hall of Fame per-bracket sub-tabs (T1..T5), along the top of the HoF page ----
    const int hofX = x + kMargin + 16;
    const int hofW = WINDOW_WIDTH - 2 * kMargin - 32;
    const int hofTabW = (hofW - 4 * 4) / 5;
    for (int i = 0; i < 5; ++i)
        m_BtnHofBracket[i].ChangeButtonInfo(hofX + i * (hofTabW + 4), y + kContentY + 36, hofTabW, 20);
}

float CNewUIDuelLadder::GetLayerDepth() { return 3.35f; }
float CNewUIDuelLadder::GetKeyEventOrder() { return 3.35f; }

void CNewUIDuelLadder::SendRequest(BYTE op, BYTE arg)
{
    if (SocketClient == NULL) return;
    SocketClient->ToGameServer()->SendDuelLadderRequest(op, arg);
}

int CNewUIDuelLadder::PageCount() const
{
    if (m_EntryCount <= 0) return 1;
    return (m_EntryCount + VISIBLE_ROWS - 1) / VISIBLE_ROWS;
}

bool CNewUIDuelLadder::EntryPassesFilters(const Entry& entry) const
{
    // Class filter: the 7 class buttons {DK,DW,ELF,SUM,MG,DL,RF} map to the entry's base class
    // group (classNumber >> 2), which is ordered {DW,DK,ELF,MG,DL,SUM,RF}.
    if (m_ClassFilter != 0)
    {
        static const int s_ButtonToGroup[7] = { 1, 0, 2, 5, 3, 4, 6 };
        const int group = entry.classNumber >> 2;
        bool match = false;
        for (int i = 0; i < 7; ++i)
            if ((m_ClassFilter & (1u << i)) && s_ButtonToGroup[i] == group) { match = true; break; }
        if (!match)
            return false;
    }

    // Tier filter: derive the entry's tier from its rating (same thresholds as the server).
    if (m_TierFilter != 0)
    {
        const unsigned int r = entry.rating;
        const int tier = (r < 1200) ? 0 : (r < 1400) ? 1 : (r < 1600) ? 2 : (r < 1800) ? 3 : (r < 2000) ? 4 : 5;
        if (!(m_TierFilter & (1u << tier)))
            return false;
    }

    // Rating: minimum-threshold radio (0 = Any).
    if (m_RatingMinSel > 0)
    {
        static const unsigned int s_RatingMin[5] = { 0, 1400, 1600, 1800, 2000 };
        if (entry.rating < s_RatingMin[m_RatingMinSel])
            return false;
    }

    // Win rate: minimum-threshold radio (0 = Any). Derived from wins / (wins+losses).
    if (m_WinRateMinSel > 0)
    {
        static const int s_WinRateMin[5] = { 0, 50, 60, 70, 80 };
        const unsigned int games = entry.wins + entry.losses;
        const int wr = games > 0 ? (int)((entry.wins * 100u) / games) : 0;
        if (wr < s_WinRateMin[m_WinRateMinSel])
            return false;
    }

    // Search player: case-insensitive substring match on the character name.
    if (m_SearchText[0] != 0)
    {
        wchar_t wname[NAME_LEN + 1] = { 0 };
        ConvertNameToWide(entry.name, wname, NAME_LEN + 1);
        wchar_t ln[NAME_LEN + 1], ls[NAME_LEN + 1];
        int i = 0; for (; wname[i] && i < NAME_LEN; ++i) ln[i] = (wchar_t)towlower(wname[i]); ln[i] = 0;
        int j = 0; for (; m_SearchText[j] && j < NAME_LEN; ++j) ls[j] = (wchar_t)towlower(m_SearchText[j]); ls[j] = 0;
        if (ls[0] != 0 && wcsstr(ln, ls) == nullptr)
            return false;
    }

    // Guild: case-insensitive substring match on the guild name (guildless players never match).
    if (m_GuildText[0] != 0)
    {
        wchar_t wg[NAME_LEN + 1] = { 0 };
        ConvertNameToWide(entry.guild, wg, NAME_LEN + 1);
        wchar_t lg[NAME_LEN + 1], lq[NAME_LEN + 1];
        int i = 0; for (; wg[i] && i < NAME_LEN; ++i) lg[i] = (wchar_t)towlower(wg[i]); lg[i] = 0;
        int j = 0; for (; m_GuildText[j] && j < NAME_LEN; ++j) lq[j] = (wchar_t)towlower(m_GuildText[j]); lq[j] = 0;
        if (lq[0] != 0 && wcsstr(lg, lq) == nullptr)
            return false;
    }

    return true;
}

int CNewUIDuelLadder::BuildVisibleRows(int* outIndices) const
{
    int count = 0;
    for (int e = 0; e < m_EntryCount; ++e)
        if (EntryPassesFilters(m_Entries[e]))
            outIndices[count++] = e;
    return count;
}

void CNewUIDuelLadder::Toggle()
{
    if (IsVisible())
    {
        HideSearchInput();
        HideGuildInput();
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_DUELLADDER);
    }
    else
    {
        g_pNewUISystem->Show(SEASON3B::INTERFACE_DUELLADDER);
        SendRequest(DUEL_OP_RANKINGS, m_CurrentBracket);
        SendRequest(DUEL_OP_PROFILE, 0);
        SendRequest(DUEL_OP_WAITING_LIST, 0);
    }
}

void CNewUIDuelLadder::SetTopData(BYTE bracket, BYTE count, const BYTE* data, int dataLen)
{
    m_CurrentBracket = bracket == 0 ? m_CurrentBracket : bracket;
    m_EntryCount = 0;
    m_SelectedRow = -1;
    m_HoveredRow = -1;
    m_Page = 0;
    if (data == NULL) return;

    int offset = 0;
    for (int i = 0; i < count && i < MAX_ENTRIES; ++i)
    {
        if (offset + ENTRY_BYTES > dataLen) break;
        Entry& e = m_Entries[i];
        std::memset(&e, 0, sizeof(e));
        std::memcpy(e.name, data + offset, NAME_LEN); e.name[NAME_LEN] = 0;
        e.classNumber = data[offset + NAME_LEN];
        e.rating = ReadUInt32(data + offset + NAME_LEN + 1);
        e.wins = ReadUInt32(data + offset + NAME_LEN + 5);
        e.losses = ReadUInt32(data + offset + NAME_LEN + 9);
        e.streak = 0;
        e.guild[0] = 0;
        offset += ENTRY_BYTES;
        m_EntryCount++;
    }
}

// op 5: guild names for the top rows, sent right after op 0 (SetTopData) and matched by index.
void CNewUIDuelLadder::SetTopGuilds(BYTE count, const BYTE* data, int dataLen)
{
    if (data == NULL) return;
    int offset = 0;
    for (int i = 0; i < count && i < m_EntryCount && i < MAX_ENTRIES; ++i)
    {
        if (offset + NAME_LEN > dataLen) break;
        std::memcpy(m_Entries[i].guild, data + offset, NAME_LEN);
        m_Entries[i].guild[NAME_LEN] = 0;
        offset += NAME_LEN;
    }
}

void CNewUIDuelLadder::SetProfileData(BYTE bracket, BYTE tier, unsigned int rating, unsigned int wins, unsigned int losses, unsigned short rankInBracket)
{
    m_ProfileBracket = bracket;
    m_ProfileTier = tier;
    m_ProfileRating = rating;
    m_ProfileWins = wins;
    m_ProfileLosses = losses;
    m_ProfileRank = rankInBracket;
}

void CNewUIDuelLadder::SetWaitingData(BYTE selfListed, BYTE count, const BYTE* data, int dataLen)
{
    m_IsWaiting = selfListed != 0;
    m_WaitingCount = 0;
    if (data == NULL) return;

    const int WB = NAME_LEN + 1 + 4 + 1 + 1 + 4; // name+class+rating+tier+bracket+waitSeconds
    int offset = 0;
    for (int i = 0; i < count && i < MAX_WAITING; ++i)
    {
        if (offset + WB > dataLen) break;
        WaitingEntry& w = m_Waiting[i];
        std::memset(&w, 0, sizeof(w));
        std::memcpy(w.name, data + offset, NAME_LEN); w.name[NAME_LEN] = 0;
        w.classNumber = data[offset + NAME_LEN];
        w.rating = ReadUInt32(data + offset + NAME_LEN + 1);
        w.tier = data[offset + NAME_LEN + 5];
        w.bracket = data[offset + NAME_LEN + 6];
        w.waitSeconds = ReadUInt32(data + offset + NAME_LEN + 7);
        offset += WB;
        m_WaitingCount++;
    }

    // Keep the scroll offset valid if the list shrank since the last refresh.
    const int waitMax = (m_WaitingCount > WAITING_VISIBLE) ? (m_WaitingCount - WAITING_VISIBLE) : 0;
    if (m_WaitingScroll > waitMax) m_WaitingScroll = waitMax;
    if (m_WaitingScroll < 0) m_WaitingScroll = 0;
}

void CNewUIDuelLadder::SetHistoryData(BYTE count, const BYTE* data, int dataLen)
{
    m_HistoryCount = 0;
    if (data == NULL) return;

    const int HB = NAME_LEN + 1 + 1 + 1 + 4 + 1 + 4; // opp+result+my+opp+ratingChange+bracket+when
    int offset = 0;
    for (int i = 0; i < count && i < MAX_HISTORY; ++i)
    {
        if (offset + HB > dataLen) break;
        HistoryEntry& h = m_History[i];
        std::memset(&h, 0, sizeof(h));
        std::memcpy(h.opponent, data + offset, NAME_LEN); h.opponent[NAME_LEN] = 0;
        h.result = data[offset + NAME_LEN];
        h.myScore = data[offset + NAME_LEN + 1];
        h.oppScore = data[offset + NAME_LEN + 2];
        h.ratingChange = (int)ReadUInt32(data + offset + NAME_LEN + 3);
        h.bracket = data[offset + NAME_LEN + 7];
        h.when = ReadUInt32(data + offset + NAME_LEN + 8);
        offset += HB;
        m_HistoryCount++;
    }
}

void CNewUIDuelLadder::SetHallOfFameData(BYTE count, const BYTE* data, int dataLen)
{
    m_HofCount = 0;
    if (data == NULL) return;

    const int FB = 1 + 1 + 1 + NAME_LEN + 1 + 4 + 4 + 4; // season+bracket+rank+name+class+rating+wins+losses = 26
    int offset = 0;
    for (int i = 0; i < count && i < MAX_HOF; ++i)
    {
        if (offset + FB > dataLen) break;
        HofEntry& f = m_Hof[i];
        std::memset(&f, 0, sizeof(f));
        f.season = data[offset];
        f.bracket = data[offset + 1];
        f.rank = data[offset + 2];
        std::memcpy(f.name, data + offset + 3, NAME_LEN); f.name[NAME_LEN] = 0;
        f.classNumber = data[offset + 3 + NAME_LEN];
        f.rating = ReadUInt32(data + offset + 4 + NAME_LEN);
        f.wins = ReadUInt32(data + offset + 8 + NAME_LEN);
        f.losses = ReadUInt32(data + offset + 12 + NAME_LEN);
        offset += FB;
        m_HofCount++;
    }
}

bool CNewUIDuelLadder::Update()
{
    if (!IsVisible()) { HideSearchInput(); HideGuildInput(); return true; }

    // While a modal box (duel request popup) OR the chat input box is open, don't process our own
    // controls so the player can answer the popup / type and close chat with the ladder still open.
    if (!CNewUIMessageBoxMng::GetInstance()->IsEmpty()
        || (g_pChatInputBox != nullptr && g_pChatInputBox->IsVisible()))
    {
        HideSearchInput();
        HideGuildInput();
        return true;
    }

    // The SEARCH PLAYER / GUILD edits live only on the Rankings tab; hide them everywhere else.
    if (m_CurrentTab == TAB_RANKINGS) { UpdateSearchInput(); UpdateGuildInput(); }
    else                              { HideSearchInput(); HideGuildInput(); }

    extern bool MouseLButtonPush; // global left-button state (ZzzOpenglUtil), used for row selection

    // Window controls.
    if (m_BtnClose.UpdateMouseEvent()) { HideSearchInput(); HideGuildInput(); g_pNewUISystem->Hide(SEASON3B::INTERFACE_DUELLADDER); return false; }

    // Tabs.
    for (int i = 0; i < TAB_COUNT; ++i)
    {
        if (m_BtnTab[i].UpdateMouseEvent())
        {
            m_CurrentTab = i;
            if (i == TAB_RANKINGS) SendRequest(DUEL_OP_RANKINGS, m_CurrentBracket);
            else if (i == TAB_PROFILE) SendRequest(DUEL_OP_PROFILE, 0);
            else if (i == TAB_HISTORY) SendRequest(DUEL_OP_HISTORY, 0);
            else if (i == TAB_HOF) SendRequest(DUEL_OP_HALLOFFAME, m_HofBracket);
            // Season Rewards renders client-side from the already-loaded profile (no request).
            return false;
        }
    }

    if (m_CurrentTab == TAB_RANKINGS)
    {
        // (BRACKET button removed — 1v1 only; m_CurrentBracket stays 1.)

        // Class / tier filters toggle client-side on the loaded top list (multi-select).
        for (int i = 0; i < 7; ++i)
            if (m_BtnClass[i].UpdateMouseEvent()) { m_ClassFilter ^= (1u << i); m_SelectedRow = -1; return false; }
        for (int i = 0; i < 6; ++i)
            if (m_BtnTier[i].UpdateMouseEvent()) { m_TierFilter ^= (1u << i); m_SelectedRow = -1; return false; }
        // Rating / win-rate are single-select min thresholds (radio: re-click "Any" to clear).
        for (int i = 0; i < 5; ++i)
            if (m_BtnRating[i].UpdateMouseEvent()) { m_RatingMinSel = i; m_SelectedRow = -1; m_Page = 0; return false; }
        for (int i = 0; i < 5; ++i)
            if (m_BtnWinRate[i].UpdateMouseEvent()) { m_WinRateMinSel = i; m_SelectedRow = -1; m_Page = 0; return false; }
        m_BtnSeason.UpdateMouseEvent();

        // Click into the SEARCH PLAYER box → focus the edit control so the player can type.
        {
            const int sx = m_Pos.x + kFiltersX + 8 + 2;
            const int sy = m_Pos.y + kfSearchY + 2;
            if (m_pSearchInput != nullptr && SEASON3B::CheckMouseIn(sx, sy, 112, 15) && MouseLButtonPush)
            {
                m_pSearchInput->GiveFocus();
                MouseLButtonPush = false;
                return false;
            }
        }
        // "Clear" wipes the search text (filtering is live as you type, so there's no Apply).
        if (m_BtnSearchPlayer.UpdateMouseEvent())
        {
            if (m_pSearchInput != nullptr) m_pSearchInput->SetText(L"");
            m_SearchText[0] = 0; m_Page = 0; m_SelectedRow = -1;
            return false;
        }
        // Click into the GUILD box → focus the edit control so the player can type.
        {
            const int gx = m_Pos.x + kFiltersX + 8 + 2;
            const int gy = m_Pos.y + kfGuildY + 2;
            if (m_pGuildInput != nullptr && SEASON3B::CheckMouseIn(gx, gy, 112, 15) && MouseLButtonPush)
            {
                m_pGuildInput->GiveFocus();
                MouseLButtonPush = false;
                return false;
            }
        }
        // GUILD "X" clears the guild text.
        if (m_BtnSearchGuild.UpdateMouseEvent())
        {
            if (m_pGuildInput != nullptr) m_pGuildInput->SetText(L"");
            m_GuildText[0] = 0; m_Page = 0; m_SelectedRow = -1;
            return false;
        }
        if (m_BtnResetFilters.UpdateMouseEvent()) { m_ClassFilter = 0; m_TierFilter = 0; m_RatingMinSel = 0; m_WinRateMinSel = 0; m_CurrentBracket = 1; m_SelectedRow = -1; m_Page = 0; if (m_pSearchInput) m_pSearchInput->SetText(L""); m_SearchText[0] = 0; if (m_pGuildInput) m_pGuildInput->SetText(L""); m_GuildText[0] = 0; SendRequest(DUEL_OP_RANKINGS, 1); return false; }
        if (m_BtnApplyFilters.UpdateMouseEvent()) { SendRequest(DUEL_OP_RANKINGS, m_CurrentBracket); return false; }

        // Pagination.
        if (m_BtnFirst.UpdateMouseEvent()) { m_Page = 0; return false; }
        if (m_BtnPrev.UpdateMouseEvent()) { if (m_Page > 0) m_Page--; return false; }
        if (m_BtnNext.UpdateMouseEvent()) { if (m_Page < PageCount() - 1) m_Page++; return false; }
        if (m_BtnLast.UpdateMouseEvent()) { m_Page = PageCount() - 1; return false; }

        // Ranking row hover / selection.
        const int tableX = m_Pos.x + kCenterX + 6;
        const int tableW = kCenterW - 12;
        const int rowTop = m_Pos.y + kContentY + 42;
        const int rowH = 14;
        m_HoveredRow = -1;
        int vis[MAX_ENTRIES];
        const int vcount = BuildVisibleRows(vis);
        const int first = m_Page * VISIBLE_ROWS;
        for (int r = 0; r < VISIBLE_ROWS; ++r)
        {
            const int vr = first + r;
            if (vr >= vcount) break;
            const int idx = vis[vr];
            const int ry = rowTop + r * rowH;
            if (CheckMouseIn(tableX, ry, tableW, rowH))
            {
                m_HoveredRow = idx;
                if (MouseLButtonPush) { m_SelectedRow = idx; MouseLButtonPush = false; } // consume = single select
                break;
            }
        }

        // Details CHALLENGE: challenge the selected ranking player. The server maps the
        // (bracket, rowIndex) back to the character, finds them online and routes through the
        // standard duel request (so the target accepts; same-map rules apply). bracket fits in
        // bits 5-7, rowIndex in bits 0-4 (top list is <=10 rows).
        if (m_BtnChallenge.UpdateMouseEvent())
        {
            if (m_SelectedRow >= 0 && m_SelectedRow < m_EntryCount)
                SendRequest(DUEL_OP_CHALLENGE_RANK, (BYTE)(((m_CurrentBracket & 0x07) << 5) | (m_SelectedRow & 0x1F)));
            return false;
        }
        m_BtnViewGear.UpdateMouseEvent();
        m_BtnCompare.UpdateMouseEvent();

        // Waiting-to-fight (Phase 2, server-backed). List/Remove/Refresh request the server, which
        // replies with op 2 -> SetWaitingData (which also drives m_IsWaiting). A waiting-row "Fight"
        // challenges that player by index via op 5; the server routes it through the standard duel
        // request so the target gets the normal accept/decline invitation.
        if (m_BtnListMe.UpdateMouseEvent()) { SendRequest(DUEL_OP_LIST_ME, m_CurrentBracket); return false; }
        if (m_BtnRemoveMe.UpdateMouseEvent()) { SendRequest(DUEL_OP_REMOVE_ME, 0); return false; }
        if (m_BtnRefreshWaiting.UpdateMouseEvent()) { SendRequest(DUEL_OP_WAITING_LIST, 0); return false; }
        // Scroll the waiting list a page at a time (so 20+ waiting players are all reachable).
        const int waitMax = (m_WaitingCount > WAITING_VISIBLE) ? (m_WaitingCount - WAITING_VISIBLE) : 0;
        if (m_BtnWaitUp.UpdateMouseEvent()) { m_WaitingScroll -= WAITING_VISIBLE; if (m_WaitingScroll < 0) m_WaitingScroll = 0; return false; }
        if (m_BtnWaitDown.UpdateMouseEvent()) { m_WaitingScroll += WAITING_VISIBLE; if (m_WaitingScroll > waitMax) m_WaitingScroll = waitMax; return false; }
        for (int i = 0; i < WAITING_VISIBLE; ++i)
            if (m_BtnWaitingChallenge[i].UpdateMouseEvent())
            {
                const int di = m_WaitingScroll + i;
                if (di < m_WaitingCount) SendRequest(DUEL_OP_CHALLENGE, (BYTE)di);
                return false;
            }
    }
    else if (m_CurrentTab == TAB_HOF)
    {
        // Hall of Fame per-bracket sub-tabs: switch the tier and re-request that tier's champions.
        for (int i = 0; i < 5; ++i)
            if (m_BtnHofBracket[i].UpdateMouseEvent())
            {
                m_HofBracket = (BYTE)(i + 1);
                SendRequest(DUEL_OP_HALLOFFAME, m_HofBracket);
                return false;
            }
    }

    return false;
}

bool CNewUIDuelLadder::UpdateMouseEvent()
{
    // Yield to an open modal message box (duel request popup) or the chat input box. This window is
    // large enough to cover them, so without this it would swallow their clicks and the player
    // couldn't answer the popup / close chat without closing the ladder first.
    if (!CNewUIMessageBoxMng::GetInstance()->IsEmpty()
        || (g_pChatInputBox != nullptr && g_pChatInputBox->IsVisible()))
        return true;
    if (!CheckMouseIn(m_Pos.x, m_Pos.y, WINDOW_WIDTH, WINDOW_HEIGHT))
        return true;
    return false;
}

bool CNewUIDuelLadder::UpdateKeyEvent()
{
    // Let an open modal box (duel request popup) or the chat input box handle ESC first, rather
    // than closing the ladder (so ESC closes the chat, not the window underneath it).
    if (!CNewUIMessageBoxMng::GetInstance()->IsEmpty()
        || (g_pChatInputBox != nullptr && g_pChatInputBox->IsVisible()))
        return true;
    if (IsVisible() && IsPress(VK_ESCAPE))
    {
        HideSearchInput();
        HideGuildInput();
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_DUELLADDER);
        SetFocus(g_hWnd);
        return false;
    }
    return true;
}

// ---- shared visual helpers ---------------------------------------------------------------

void CNewUIDuelLadder::RenderFrame(int x, int y, int width, int height, int corner)
{
    if (width <= 0 || height <= 0) return;
    int c = corner;
    if (c > width / 2)  c = width / 2;
    if (c > height / 2) c = height / 2;
    if (c < 2) c = 2;
    const float fx = float(x), fy = float(y), fw = float(width), fh = float(height), fc = float(c);

    glColor4f(1.f, 1.f, 1.f, 1.f);
    RenderImage(IMAGE_DUEL_FRAME_TL, fx, fy, fc, fc);
    RenderImage(IMAGE_DUEL_FRAME_TR, fx + fw - fc, fy, fc, fc);
    RenderImage(IMAGE_DUEL_FRAME_BL, fx, fy + fh - fc, fc, fc);
    RenderImage(IMAGE_DUEL_FRAME_BR, fx + fw - fc, fy + fh - fc, fc, fc);
    if (fw - 2.f * fc > 0.f)
    {
        RenderImage(IMAGE_DUEL_FRAME_TOP, fx + fc, fy, fw - 2.f * fc, fc);
        RenderImage(IMAGE_DUEL_FRAME_BOTTOM, fx + fc, fy + fh - fc, fw - 2.f * fc, fc);
    }
    if (fh - 2.f * fc > 0.f)
    {
        RenderImage(IMAGE_DUEL_FRAME_LEFT, fx, fy + fc, fc, fh - 2.f * fc);
        RenderImage(IMAGE_DUEL_FRAME_RIGHT, fx + fw - fc, fy + fc, fc, fh - 2.f * fc);
    }
}

void CNewUIDuelLadder::RenderSectionTitle(int x, int y, int width, const wchar_t* title)
{
    RenderUiRect(x, y - 2, width, 16, 0.070f, 0.095f, 0.150f, 0.62f);
    RenderUiRect(x, y + 14, width, 1, 0.46f, 0.40f, 0.24f, 0.60f);
    g_pRenderText->SetFont(g_hFontBold);
    RenderTextEx(x + 6, y + 1, width - 12, title, 240, 210, 130, 255, RT3_SORT_LEFT);
    g_pRenderText->SetFont(g_hFont);
}

void CNewUIDuelLadder::RenderCard(int x, int y, int width, int height, float fillAlpha)
{
    const float a = fillAlpha > 0.0f ? fillAlpha : 0.34f;
    // Deeper recessed inset: a noticeably darker fill than the brown window body so content
    // reads as sunk in, a dark inner top/left shadow for depth, then a warm-gold top highlight
    // just under the frame. The contrast is what gives the text separation from the background.
    RenderUiRect(x + 2, y + 2, width - 4, height - 4, 0.050f, 0.065f, 0.105f, a); // navy inset
    RenderUiRect(x + 3, y + 3, width - 6, 2, 0.010f, 0.014f, 0.026f, 0.60f);   // inner top shadow
    RenderUiRect(x + 3, y + 3, 2, height - 6, 0.010f, 0.014f, 0.026f, 0.45f);  // inner left shadow
    RenderUiRect(x + 5, y + 6, width - 10, 1, 0.40f, 0.45f, 0.62f, 0.45f);     // cool top highlight
    RenderFrame(x, y, width, height, 14);
}

void CNewUIDuelLadder::RenderButtonVisual(CNewUIButton& button, const wchar_t* text, bool active, bool enabled, bool accentGreen)
{
    const POINT& p = button.GetPos();
    const POINT& s = button.GetSize();
    const int x = p.x, y = p.y, w = s.x, h = s.y;
    if (w <= 2 || h <= 2) return;

    const bool hot = enabled && SEASON3B::CheckMouseIn(x, y, w, h);

    // Navy/gold palette: dark navy edge, navy face (blue when selected, green for the APPLY accent),
    // gold top/left bevel + dark bottom/right, gold underline on the active control.
    RenderUiRect(x, y, w, h, 0.020f, 0.030f, 0.050f, 1.0f);

    float fr, fg, fb;
    if (!enabled)         { fr = 0.080f; fg = 0.100f; fb = 0.140f; }
    else if (accentGreen) { fr = hot ? 0.150f : 0.110f; fg = hot ? 0.430f : 0.360f; fb = hot ? 0.235f : 0.195f; }
    else if (active)      { fr = 0.150f; fg = 0.330f; fb = 0.600f; }
    else if (hot)         { fr = 0.155f; fg = 0.205f; fb = 0.315f; }
    else                  { fr = 0.105f; fg = 0.140f; fb = 0.220f; }
    RenderUiRect(x + 1, y + 1, w - 2, h - 2, fr, fg, fb, 1.0f);

    if (enabled)
    {
        RenderUiRect(x + 1, y + 1, w - 2, 1, 0.620f, 0.520f, 0.300f, 0.85f);   // gold top
        RenderUiRect(x + 1, y + 1, 1, h - 2, 0.620f, 0.520f, 0.300f, 0.85f);   // gold left
        RenderUiRect(x + 1, y + h - 2, w - 2, 1, 0.020f, 0.030f, 0.050f, 0.85f);
        RenderUiRect(x + w - 2, y + 1, 1, h - 2, 0.020f, 0.030f, 0.050f, 0.85f);
    }

    if (active && w > 14)
        RenderUiRect(x + 5, y + h - 4, w - 10, 2, 0.95f, 0.80f, 0.40f, 0.95f);

    BYTE r = enabled ? (active ? 255 : 238) : 120;
    BYTE g = enabled ? (active ? 240 : 222) : 132;
    BYTE b = enabled ? (active ? 205 : 170) : 152;
    BYTE a = enabled ? 255 : 175;
    RenderTextEx(x, y + (h - 9) / 2, w, text, r, g, b, a, RT3_SORT_CENTER);
}

void CNewUIDuelLadder::RenderCloseButton()
{
    const POINT& p = m_BtnClose.GetPos();
    const POINT& s = m_BtnClose.GetSize();
    const int x = p.x, y = p.y, w = s.x, h = s.y;
    const bool hot = SEASON3B::CheckMouseIn(x, y, w, h);

    RenderUiRect(x, y, w, h, 0.020f, 0.030f, 0.050f, 1.0f);
    RenderUiRect(x + 1, y + 1, w - 2, h - 2, hot ? 0.155f : 0.105f, hot ? 0.205f : 0.140f, hot ? 0.315f : 0.220f, 1.0f);
    RenderUiRect(x + 1, y + 1, w - 2, 1, 0.620f, 0.520f, 0.300f, 0.85f);
    RenderUiRect(x + 1, y + 1, 1, h - 2, 0.620f, 0.520f, 0.300f, 0.85f);
    RenderUiRect(x + 1, y + h - 2, w - 2, 1, 0.020f, 0.030f, 0.050f, 0.85f);
    RenderUiRect(x + w - 2, y + 1, 1, h - 2, 0.020f, 0.030f, 0.050f, 0.85f);
    g_pRenderText->SetFont(g_hFontBold);
    RenderTextEx(x, y + (h - 9) / 2, w, L"X", 255, hot ? 210 : 184, hot ? 120 : 96, 255, RT3_SORT_CENTER);
    g_pRenderText->SetFont(g_hFont);
}

void CNewUIDuelLadder::RenderTabBar()
{
    // Divider rule spanning the window just under the tab row, so the buttons read as a real tab
    // bar; the active tab's brighter face + gold underline then sit on this line like a selected
    // tab connecting to the content below.
    RenderUiRect(m_Pos.x + 12, m_Pos.y + kTabY + kTabH, WINDOW_WIDTH - 24, 1, 0.46f, 0.37f, 0.22f, 0.70f);
    for (int i = 0; i < TAB_COUNT; ++i)
        RenderButtonVisual(m_BtnTab[i], s_TabNames[i], m_CurrentTab == i, true);
}

void CNewUIDuelLadder::RenderPendingNotice(int x, int y, int width, int height, const wchar_t* line1, const wchar_t* line2)
{
    RenderCard(x, y, width, height, 0.20f);
    const int cy = y + height / 2 - 10;
    RenderTextEx(x, cy, width, line1, 224, 200, 150, 255, RT3_SORT_CENTER);
    if (line2) RenderTextEx(x, cy + 16, width, line2, 150, 138, 116, 230, RT3_SORT_CENTER);
}

// ---- rankings tab ------------------------------------------------------------------------

void CNewUIDuelLadder::RenderFiltersPanel(int x, int y, int width, int height)
{
    RenderCard(x, y, width, height, 0.22f);
    const int lx = x + 8;
    const int lw = width - 16;
    const int by = y - kContentY;   // == m_Pos.y; the kf*Y offsets are measured from here
    RenderSectionTitle(lx, y + 6, lw, L"FILTERS");

    auto label = [&](int rowY, const wchar_t* t)
    { RenderTextEx(lx, by + rowY - 12, lw, t, 196, 178, 142, 255, RT3_SORT_LEFT); };
    auto box = [&](int bx, int rowY, int bw, const wchar_t* placeholder)
    {
        RenderUiRect(bx, by + rowY, bw, 18, 0.035f, 0.050f, 0.085f, 0.9f);
        RenderTextEx(bx + 5, by + rowY + 4, bw - 8, placeholder, 130, 120, 100, 255, RT3_SORT_LEFT);
    };

    label(kfSeasonY, L"SEASON");
    RenderButtonVisual(m_BtnSeason, L"Current Season", false, false);

    // BRACKET row removed — 1v1 is the only bracket, so the field is unnecessary.

    label(kfClassY, L"CLASS");
    for (int i = 0; i < 7; ++i)
        RenderButtonVisual(m_BtnClass[i], s_ClassNames[i], ((m_ClassFilter >> i) & 1) != 0, true);

    label(kfTierY, L"RANK TIER");
    for (int i = 0; i < 6; ++i)
        RenderButtonVisual(m_BtnTier[i], s_DuelTierNames[i], ((m_TierFilter >> i) & 1) != 0, true);

    static const wchar_t* s_RatingLbl[5] = { L"Any", L"1.4k", L"1.6k", L"1.8k", L"2.0k" };
    static const wchar_t* s_WinRateLbl[5] = { L"Any", L"50%", L"60%", L"70%", L"80%" };
    label(kfRatingY, L"RATING (min)");
    for (int i = 0; i < 5; ++i)
        RenderButtonVisual(m_BtnRating[i], s_RatingLbl[i], m_RatingMinSel == i, true);
    label(kfWinRateY, L"WIN RATE (min)");
    for (int i = 0; i < 5; ++i)
        RenderButtonVisual(m_BtnWinRate[i], s_WinRateLbl[i], m_WinRateMinSel == i, true);

    // SEARCH PLAYER: live name filter. The dark field + placeholder are drawn here; the native
    // edit control renders its typed text/caret on top (see UpdateSearchInput / EnsureSearchInput).
    label(kfSearchY, L"SEARCH PLAYER");
    box(lx, kfSearchY, lw - 34, m_SearchText[0] ? L"" : L"Type a name...");
    if (m_pSearchInput != nullptr && m_SearchInputShown) m_pSearchInput->Render();
    RenderButtonVisual(m_BtnSearchPlayer, L"X", false, true); // clear the search text

    // GUILD: live guild-name filter (server sends a guild per ranking row in the op-5 packet).
    label(kfGuildY, L"GUILD");
    box(lx, kfGuildY, lw - 34, m_GuildText[0] ? L"" : L"Type a guild...");
    if (m_pGuildInput != nullptr && m_GuildInputShown) m_pGuildInput->Render();
    RenderButtonVisual(m_BtnSearchGuild, L"X", false, true);

    RenderButtonVisual(m_BtnResetFilters, L"RESET", false, true);
    RenderButtonVisual(m_BtnApplyFilters, L"APPLY", false, true, true); // green accent

}

void CNewUIDuelLadder::RenderRankingTable(int x, int y, int width, int height)
{
    RenderCard(x, y, width, height, 0.20f);
    const int hx = x + 6;
    const int hw = width - 12;
    RenderSectionTitle(hx, y + 8, hw, L"LADDER RANKINGS");

    // Column geometry (relative to hx; all within hw = width-12 so nothing overflows the card).
    // Columns (relative to hx; all within hw = width-12). Guild data comes from the op-5 packet.
    const int cR = hx + 3, cN = hx + 20, cG = hx + 74, cC = hx + 120, cRt = hx + 144, cW = hx + 176, cL = hx + 194, cWR = hx + 210;
    static const int sepX[6] = { 18, 72, 118, 142, 174, 208 }; // #|CHAR|GUILD|CLS|RTG|W L|WR

    const int headerY = y + 28;
    const int rowTop = y + 42;
    const int rowH = 14;
    RenderUiRect(hx, headerY, hw, 14, 0.070f, 0.095f, 0.150f, 0.95f);
    RenderTextEx(cR, headerY + 2, 16, L"#", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(cN, headerY + 2, 52, L"CHARACTER", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(cG, headerY + 2, 44, L"GUILD", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(cC, headerY + 2, 22, L"CLS", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(cRt, headerY + 2, 30, L"RTG", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(cW, headerY + 2, 16, L"W", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(cL, headerY + 2, 14, L"L", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(cWR, headerY + 2, 24, L"WR", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderUiRect(hx, headerY + 14, hw, 1, 0.42f, 0.34f, 0.20f, 0.55f); // header underline

    int vis[MAX_ENTRIES];
    const int vcount = BuildVisibleRows(vis);
    if (vcount == 0)
    {
        RenderTextEx(x, y + height / 2 - 8, width,
            m_EntryCount == 0 ? L"No ranked players found." : L"No players match the filters.",
            196, 178, 142, 255, RT3_SORT_CENTER);
        RenderTextEx(x, y + height / 2 + 8, width, L"Try changing your filters.", 150, 138, 116, 230, RT3_SORT_CENTER);
        return;
    }

    const int first = m_Page * VISIBLE_ROWS;
    int rowsShown = vcount - first; if (rowsShown > VISIBLE_ROWS) rowsShown = VISIBLE_ROWS; if (rowsShown < 0) rowsShown = 0;
    const int sepBottom = rowTop + rowsShown * rowH;

    // Vertical column dividers spanning header + visible rows (the "table grid" look).
    for (int k = 0; k < 6; ++k)
        RenderUiRect(hx + sepX[k], headerY, 1, sepBottom - headerY, 0.42f, 0.34f, 0.20f, 0.30f);

    for (int r = 0; r < rowsShown; ++r)
    {
        const int vr = first + r;
        const int idx = vis[vr];
        const int ry = rowTop + r * rowH;

        if (idx == m_SelectedRow)
            RenderUiRect(hx, ry, hw, rowH, 0.16f, 0.30f, 0.55f, 0.42f);
        else if (idx == m_HoveredRow)
            RenderUiRect(hx, ry, hw, rowH, 0.16f, 0.30f, 0.55f, 0.22f);
        else if (r & 1)
            RenderUiRect(hx, ry, hw, rowH, 1.0f, 1.0f, 1.0f, 0.04f);

        const Entry& e = m_Entries[idx];
        wchar_t name[NAME_LEN + 1] = { 0 }; ConvertNameToWide(e.name, name, NAME_LEN + 1);
        wchar_t buf[48];
        const int ty = ry + 3;
        wchar_t gname[NAME_LEN + 1] = { 0 }; ConvertNameToWide(e.guild, gname, NAME_LEN + 1);
        std::swprintf(buf, 48, L"%d", idx + 1);
        RenderTextEx(cR, ty, 16, buf, 238, 210, 142, 255, RT3_SORT_LEFT);
        RenderTextEx(cN, ty, 52, name, 232, 218, 190, 255, RT3_SORT_LEFT);
        RenderTextEx(cG, ty, 44, gname[0] ? gname : L"-", 180, 168, 140, 255, RT3_SORT_LEFT);
        RenderTextEx(cC, ty, 22, GetClassName(e.classNumber), 196, 178, 142, 255, RT3_SORT_LEFT);
        std::swprintf(buf, 48, L"%u", e.rating);
        RenderTextEx(cRt, ty, 30, buf, 238, 199, 86, 255, RT3_SORT_LEFT);
        std::swprintf(buf, 48, L"%u", e.wins);
        RenderTextEx(cW, ty, 16, buf, 130, 200, 130, 255, RT3_SORT_LEFT);
        std::swprintf(buf, 48, L"%u", e.losses);
        RenderTextEx(cL, ty, 14, buf, 210, 130, 130, 255, RT3_SORT_LEFT);
        std::swprintf(buf, 48, L"%u%%", GetWinRate(e.wins, e.losses));
        RenderTextEx(cWR, ty, 24, buf, 224, 212, 184, 255, RT3_SORT_LEFT);
    }

    // Pagination row (aligned with the First/Prev/Next/Last buttons along the card bottom).
    wchar_t pg[48];
    std::swprintf(pg, 48, L"Page %d / %d", m_Page + 1, PageCount());
    RenderTextEx(x + 6, y + height - 20, width - 12, pg, 150, 138, 116, 255, RT3_SORT_RIGHT);
    RenderButtonVisual(m_BtnFirst, L"First", false, m_Page > 0);
    RenderButtonVisual(m_BtnPrev, L"Prev", false, m_Page > 0);
    RenderButtonVisual(m_BtnNext, L"Next", false, m_Page < PageCount() - 1);
    RenderButtonVisual(m_BtnLast, L"Last", false, m_Page < PageCount() - 1);
}

void CNewUIDuelLadder::RenderPlayerDetails(int x, int y, int width, int height)
{
    RenderCard(x, y, width, height, 0.22f);
    const int lx = x + 8;
    const int lw = width - 16;
    RenderSectionTitle(lx, y + 8, lw, L"PLAYER DETAILS");

    if (m_SelectedRow < 0 || m_SelectedRow >= m_EntryCount)
    {
        RenderTextEx(x, y + height / 2 - 8, width, L"Select a ranking row", 238, 210, 142, 255, RT3_SORT_CENTER);
        RenderTextEx(x, y + height / 2 + 8, width, L"to inspect a player.", 150, 138, 116, 230, RT3_SORT_CENTER);
        return;
    }

    const Entry& e = m_Entries[m_SelectedRow];
    wchar_t name[NAME_LEN + 1] = { 0 }; ConvertNameToWide(e.name, name, NAME_LEN + 1);
    wchar_t buf[64];

    int ly = y + 28;
    RenderUiRect(lx, ly, 48, 48, 0.035f, 0.050f, 0.085f, 0.9f);
    RenderTextEx(lx, ly + 20, 48, GetClassName(e.classNumber), 196, 178, 142, 255, RT3_SORT_CENTER);
    RenderTextEx(lx + 56, ly + 4, lw - 56, name, 232, 218, 190, 255, RT3_SORT_LEFT);
    std::swprintf(buf, 64, L"Rank #%d", m_SelectedRow + 1);
    RenderTextEx(lx + 56, ly + 22, lw - 56, buf, 236, 206, 120, 255, RT3_SORT_LEFT);

    ly += 60;
    std::swprintf(buf, 64, L"%u", e.rating);
    RenderLabelValue(lx, ly, lw, L"Rating", buf, 238, 199, 86); ly += 18;
    std::swprintf(buf, 64, L"%u", e.wins + e.losses);
    RenderLabelValue(lx, ly, lw, L"Matches", buf, 228, 214, 186); ly += 18;
    std::swprintf(buf, 64, L"%u / %u", e.wins, e.losses);
    RenderLabelValue(lx, ly, lw, L"Wins / Losses", buf, 228, 214, 186); ly += 18;
    std::swprintf(buf, 64, L"%u%%", GetWinRate(e.wins, e.losses));
    RenderLabelValue(lx, ly, lw, L"Win rate", buf, 228, 214, 186); ly += 18;
    RenderLabelValue(lx, ly, lw, L"Class", GetClassName(e.classNumber), 220, 198, 156); ly += 24;

    RenderTextEx(lx, ly, lw, L"RECENT MATCHES", 196, 178, 142, 255, RT3_SORT_LEFT); ly += 16;
    RenderTextEx(lx, ly, lw, L"(pending match-history op)", 150, 138, 116, 230, RT3_SORT_LEFT);

    RenderButtonVisual(m_BtnChallenge, L"CHALLENGE", false, true);
    RenderButtonVisual(m_BtnViewGear, L"VIEW GEAR", false, false);
    RenderButtonVisual(m_BtnCompare, L"COMPARE", false, false);
}

void CNewUIDuelLadder::RenderMyStatusPanel(int x, int y, int width, int height)
{
    RenderCard(x, y, width, height, 0.22f);
    const int lx = x + 8;
    const int lw = width - 16;
    RenderSectionTitle(lx, y + 6, lw, L"MY LADDER STATUS");

    wchar_t buf[48];
    const int colW = lw / 2;
    auto pair = [&](int col, int row, const wchar_t* label, const wchar_t* value, BYTE vr, BYTE vg, BYTE vb)
    {
        const int px = lx + col * colW;
        RenderTextEx(px, row, 50, label, 196, 178, 142, 255, RT3_SORT_LEFT);
        RenderTextEx(px + 50, row, colW - 54, value, vr, vg, vb, 255, RT3_SORT_LEFT);
    };
    const int r0 = y + 28, r1 = y + 46, r2 = y + 64;
    std::swprintf(buf, 48, L"#%u", (unsigned)m_ProfileRank);
    pair(0, r0, L"Rank", buf, 238, 199, 86);
    std::swprintf(buf, 48, L"%u", m_ProfileRating);
    pair(1, r0, L"Rating", buf, 238, 199, 86);
    pair(0, r1, L"Tier", GetTierName(m_ProfileTier), 236, 206, 120);
    std::swprintf(buf, 48, L"%u/%u", m_ProfileWins, m_ProfileLosses);
    pair(1, r1, L"W/L", buf, 228, 214, 186);
    std::swprintf(buf, 48, L"%u%%", GetWinRate(m_ProfileWins, m_ProfileLosses));
    pair(0, r2, L"Win%", buf, 228, 214, 186);
    pair(1, r2, L"Avail", m_IsWaiting ? L"Waiting" : L"Not Listed",
        m_IsWaiting ? 130 : 240, m_IsWaiting ? 200 : 170, m_IsWaiting ? 130 : 110);
}

void CNewUIDuelLadder::RenderWaitingPanel(int x, int y, int width, int height)
{
    RenderCard(x, y, width, height, 0.22f);
    const int lx = x + 8;
    const int lw = width - 16;

    // Title carries the total count; a long list scrolls a page at a time via the ^/v arrows.
    const bool scrollable = m_WaitingCount > WAITING_VISIBLE;
    wchar_t title[40];
    if (m_WaitingCount > 0) std::swprintf(title, 40, L"WAITING TO FIGHT (%d)", m_WaitingCount);
    else                    std::swprintf(title, 40, L"WAITING TO FIGHT");
    RenderSectionTitle(lx, y + 8, scrollable ? lw - 40 : lw, title);

    RenderButtonVisual(m_BtnListMe, L"LIST ME", !m_IsWaiting, !m_IsWaiting);
    RenderButtonVisual(m_BtnRemoveMe, L"REMOVE", false, m_IsWaiting);
    RenderButtonVisual(m_BtnRefreshWaiting, L"Refresh", false, true);

    const int rowTop = y + 48;
    if (m_WaitingCount == 0)
    {
        // Nothing waiting: just the hint, no empty challenge boxes.
        RenderTextEx(lx, rowTop + 6, lw, L"No players waiting.", 150, 138, 116, 230, RT3_SORT_LEFT);
        return;
    }

    int scroll = m_WaitingScroll;
    const int waitMax = scrollable ? (m_WaitingCount - WAITING_VISIBLE) : 0;
    if (scroll > waitMax) scroll = waitMax;
    if (scroll < 0) scroll = 0;
    if (scrollable)
    {
        RenderButtonVisual(m_BtnWaitUp, L"^", false, scroll > 0);
        RenderButtonVisual(m_BtnWaitDown, L"v", false, scroll < waitMax);
    }

    for (int i = 0; i < WAITING_VISIBLE; ++i)
    {
        const int di = scroll + i;
        if (di >= m_WaitingCount) continue;   // only draw a Fight button for an occupied slot
        const WaitingEntry& w = m_Waiting[di];
        wchar_t name[NAME_LEN + 1] = { 0 }; ConvertNameToWide(w.name, name, NAME_LEN + 1);
        wchar_t buf[48];
        const int ry = rowTop + i * 14;
        RenderTextEx(lx, ry, 70, name, 232, 218, 190, 255, RT3_SORT_LEFT);
        RenderTextEx(lx + 70, ry, 26, GetClassName(w.classNumber), 196, 178, 142, 255, RT3_SORT_LEFT);
        std::swprintf(buf, 48, L"%u", w.rating);
        RenderTextEx(lx + 98, ry, 34, buf, 238, 199, 86, 255, RT3_SORT_LEFT);
        RenderButtonVisual(m_BtnWaitingChallenge[i], L"Fight", false, true);
    }
}

void CNewUIDuelLadder::RenderRankingsTab()
{
    const int x = m_Pos.x, y = m_Pos.y;
    const int filtersH = kFiltersBottom - kContentY;   // full-height left column
    const int colH = kColBottom - kContentY;           // center + right columns
    RenderFiltersPanel(x + kFiltersX, y + kContentY, kFiltersW, filtersH);
    RenderRankingTable(x + kCenterX, y + kContentY, kCenterW, colH);
    RenderPlayerDetails(x + kDetailsX, y + kContentY, kDetailsW, colH);

    const int botH = kBottomBottom - kBottomY;
    RenderMyStatusPanel(x + kStatusX, y + kBottomY, kStatusW, botH);
    RenderWaitingPanel(x + kWaitX, y + kBottomY, kWaitW, botH);
}

void CNewUIDuelLadder::RenderProfileTab()
{
    const int x = m_Pos.x, y = m_Pos.y;
    const int w = WINDOW_WIDTH - 2 * kMargin;
    RenderCard(x + kMargin, y + kContentY, w, kBottomBottom - kContentY, 0.22f);
    const int lx = x + kMargin + 16;
    const int lw = w - 32;
    RenderSectionTitle(lx, y + kContentY + 12, lw, L"MY DUEL PROFILE");

    wchar_t heroName[NAME_LEN + 1] = { 0 };
    extern CHARACTER* Hero;
    if (Hero != NULL) ConvertNameToWide(Hero->ID, heroName, NAME_LEN + 1);
    else { heroName[0] = L'-'; heroName[1] = 0; }

    // Stat table: STAT | VALUE with a header band, row stripes and a column divider.
    const int tableX = lx;
    const int tableW = 420;
    const int statW = 150;            // left column width
    const int valX = tableX + statW;
    const int rowH = 24;
    int ly = y + kContentY + 44;

    RenderUiRect(tableX, ly, tableW, 18, 0.070f, 0.095f, 0.150f, 0.95f);
    RenderTextEx(tableX + 8, ly + 3, statW - 12, L"STAT", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(valX + 8, ly + 3, tableW - statW - 12, L"VALUE", 200, 182, 142, 255, RT3_SORT_LEFT);
    ly += 18;
    const int bodyTop = ly;

    struct Row { const wchar_t* label; wchar_t value[64]; BYTE r, g, b; };
    Row rows[7];
    int n = 0;
    auto add = [&](const wchar_t* lbl, BYTE r, BYTE g, BYTE b) -> wchar_t* { rows[n].label = lbl; rows[n].r = r; rows[n].g = g; rows[n].b = b; return rows[n++].value; };
    std::swprintf(add(L"Character", 228, 214, 186), 64, L"%ls", heroName);
    std::swprintf(add(L"Bracket", 220, 198, 156), 64, L"%ls", L"1v1");
    std::swprintf(add(L"Tier", 238, 210, 142), 64, L"%ls", GetTierName(m_ProfileTier));
    std::swprintf(add(L"Rating", 238, 199, 86), 64, L"%u", m_ProfileRating);
    std::swprintf(add(L"Rank", 238, 199, 86), 64, L"#%u", (unsigned)m_ProfileRank);
    std::swprintf(add(L"Wins / Losses", 228, 214, 186), 64, L"%u / %u", m_ProfileWins, m_ProfileLosses);
    std::swprintf(add(L"Win rate", 228, 214, 186), 64, L"%u%%", GetWinRate(m_ProfileWins, m_ProfileLosses));

    for (int i = 0; i < n; ++i)
    {
        if (i & 1) RenderUiRect(tableX, ly, tableW, rowH, 1.0f, 1.0f, 1.0f, 0.04f);
        RenderTextEx(tableX + 8, ly + 5, statW - 12, rows[i].label, 196, 178, 142, 255, RT3_SORT_LEFT);
        RenderTextEx(valX + 8, ly + 5, tableW - statW - 12, rows[i].value, rows[i].r, rows[i].g, rows[i].b, 255, RT3_SORT_LEFT);
        ly += rowH;
    }

    // Table outline + column divider.
    RenderUiRect(tableX, bodyTop, tableW, 1, 0.42f, 0.34f, 0.20f, 0.45f);
    RenderUiRect(tableX, ly, tableW, 1, 0.42f, 0.34f, 0.20f, 0.45f);
    RenderUiRect(valX, bodyTop, 1, ly - bodyTop, 0.42f, 0.34f, 0.20f, 0.35f);

    ly += 18;
    RenderTextEx(tableX, ly, tableW, L"Best streak, peak rating and recent matches populate once the", 150, 138, 116, 230, RT3_SORT_LEFT);
    RenderTextEx(tableX, ly + 16, tableW, L"extended profile op is wired (pending).", 150, 138, 116, 230, RT3_SORT_LEFT);
}

void CNewUIDuelLadder::RenderHistoryTab()
{
    const int x = m_Pos.x, y = m_Pos.y;
    const int w = WINDOW_WIDTH - 2 * kMargin;
    if (m_HistoryCount == 0)
    {
        RenderPendingNotice(x + kMargin, y + kContentY, w, kBottomBottom - kContentY,
            L"No match history yet.", L"Match history op is pending server support.");
        return;
    }
    RenderCard(x + kMargin, y + kContentY, w, kBottomBottom - kContentY, 0.22f);
    const int lx = x + kMargin + 12;
    RenderSectionTitle(lx, y + kContentY + 12, w - 24, L"MATCH HISTORY");
    int ly = y + kContentY + 36;
    for (int i = 0; i < m_HistoryCount && i < HISTORY_VISIBLE; ++i)
    {
        const HistoryEntry& h = m_History[i];
        wchar_t opp[NAME_LEN + 1] = { 0 }; ConvertNameToWide(h.opponent, opp, NAME_LEN + 1);
        wchar_t buf[48];
        RenderTextEx(lx, ly, 120, opp, 232, 218, 190, 255, RT3_SORT_LEFT);
        RenderTextEx(lx + 130, ly, 50, h.result ? L"WIN" : L"LOSS", h.result ? 130 : 210, h.result ? 200 : 130, 130, 255, RT3_SORT_LEFT);
        std::swprintf(buf, 48, L"%u-%u", h.myScore, h.oppScore);
        RenderTextEx(lx + 190, ly, 50, buf, 224, 212, 184, 255, RT3_SORT_LEFT);
        std::swprintf(buf, 48, L"%+d", h.ratingChange);
        RenderTextEx(lx + 250, ly, 60, buf, h.ratingChange >= 0 ? 130 : 210, h.ratingChange >= 0 ? 200 : 130, 130, 255, RT3_SORT_LEFT);
        ly += 18;
    }
}

void CNewUIDuelLadder::RenderRewardsTab()
{
    const int x = m_Pos.x, y = m_Pos.y;
    const int w = WINDOW_WIDTH - 2 * kMargin;
    RenderCard(x + kMargin, y + kContentY, w, kBottomBottom - kContentY, 0.22f);
    const int lx = x + kMargin + 16;
    const int lw = w - 32;
    RenderSectionTitle(lx, y + kContentY + 12, lw, L"SEASON REWARDS");

    const int rank = (int)m_ProfileRank;

    int ly = y + kContentY + 42;
    RenderTextEx(lx, ly, lw, L"The top 3 of each bracket are rewarded automatically at season end:", 196, 178, 142, 255, RT3_SORT_LEFT);
    ly += 24;

    RenderUiRect(lx, ly - 3, lw, 16, 0.070f, 0.095f, 0.150f, 0.95f);
    RenderTextEx(lx + 6, ly, 90, L"PLACE", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(lx + 110, ly, lw - 210, L"REWARD", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(lx + 360, ly, lw - 360, L"CURRENT LEADER", 200, 182, 142, 255, RT3_SORT_LEFT);
    ly += 22;

    const wchar_t* placeName[3] = { L"Top 1", L"Top 2", L"Top 3" };
    const wchar_t* rewardName[3] = { L"VIP until next season", L"VIP for 15 days", L"VIP for 7 days" };
    for (int i = 0; i < 3; ++i)
    {
        const bool isYou = rank == (i + 1);
        if (i & 1)
            RenderUiRect(lx, ly - 2, lw, 18, 1.0f, 1.0f, 1.0f, 0.04f);
        RenderTextEx(lx + 6, ly, 90, placeName[i], 238, 210, 142, 255, RT3_SORT_LEFT);
        RenderTextEx(lx + 110, ly, 240, rewardName[i], 228, 214, 186, 255, RT3_SORT_LEFT);

        // Who currently holds this place (from the loaded rankings); your own name is green.
        if (i < m_EntryCount)
        {
            wchar_t leader[NAME_LEN + 1] = { 0 };
            ConvertNameToWide(m_Entries[i].name, leader, NAME_LEN + 1);
            RenderTextEx(lx + 360, ly, lw - 360, leader,
                isYou ? 130 : 232, isYou ? 200 : 218, isYou ? 130 : 190, 255, RT3_SORT_LEFT);
        }
        else
        {
            RenderTextEx(lx + 360, ly, lw - 360, L"-", 150, 138, 116, 255, RT3_SORT_LEFT);
        }
        ly += 18;
    }

    ly += 16;
    RenderUiRect(lx, ly - 6, lw, 1, 0.42f, 0.34f, 0.20f, 0.5f);
    wchar_t buf[96];
    if (rank > 0)
        std::swprintf(buf, 96, L"Your current rank: #%d", rank);
    else
        std::swprintf(buf, 96, L"Play ranked duels this season to climb into the rewards.");
    RenderTextEx(lx, ly, lw, buf, 238, 199, 86, 255, RT3_SORT_LEFT);
    ly += 18;
    RenderTextEx(lx, ly, 70, L"Your tier:", 196, 178, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(lx + 70, ly, lw - 70, GetTierName(m_ProfileTier), 228, 214, 186, 255, RT3_SORT_LEFT);
    ly += 24;
    RenderTextEx(lx, ly, lw, L"Rewards are granted automatically when the season ends (last day of the month).", 150, 138, 116, 230, RT3_SORT_LEFT);
}

void CNewUIDuelLadder::RenderHallOfFameTab()
{
    const int x = m_Pos.x, y = m_Pos.y;
    const int w = WINDOW_WIDTH - 2 * kMargin;
    RenderCard(x + kMargin, y + kContentY, w, kBottomBottom - kContentY, 0.22f);
    const int lx = x + kMargin + 16;
    const int lw = w - 32;
    RenderSectionTitle(lx, y + kContentY + 12, lw, L"HALL OF FAME");

    // Per-bracket sub-tabs (T1..T5) — the server returns champions for the selected bracket only.
    static const wchar_t* const s_HofTab[5] = { L"Tier 1", L"Tier 2", L"Tier 3", L"Tier 4", L"Tier 5" };
    static const wchar_t* const s_HofRange[5] = {
        L"Tier 1 - 0 to 5 resets", L"Tier 2 - 6 to 15 resets", L"Tier 3 - 16 to 30 resets",
        L"Tier 4 - 31 to 50 resets", L"Tier 5 - 51+ resets" };
    for (int i = 0; i < 5; ++i)
        RenderButtonVisual(m_BtnHofBracket[i], s_HofTab[i], m_HofBracket == i + 1, true);

    const int bi = (m_HofBracket >= 1 && m_HofBracket <= 5) ? m_HofBracket - 1 : 0;
    RenderTextEx(lx, y + kContentY + 62, lw, s_HofRange[bi], 236, 206, 120, 255, RT3_SORT_LEFT);

    if (m_HofCount == 0)
    {
        RenderTextEx(lx, y + kContentY + 120, lw, L"No champions recorded for this tier yet.", 150, 138, 116, 230, RT3_SORT_CENTER);
        return;
    }

    int ly = y + kContentY + 84;
    RenderUiRect(lx, ly - 3, lw, 16, 0.070f, 0.095f, 0.150f, 0.95f);
    RenderTextEx(lx + 6, ly, 60, L"SEASON", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(lx + 80, ly, 50, L"RANK", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(lx + 140, ly, 170, L"CHAMPION", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(lx + 320, ly, 40, L"CLS", 200, 182, 142, 255, RT3_SORT_LEFT);
    RenderTextEx(lx + 366, ly, 50, L"RATING", 200, 182, 142, 255, RT3_SORT_LEFT);
    ly += 22;

    for (int i = 0; i < m_HofCount && i < HOF_VISIBLE; ++i)
    {
        const HofEntry& f = m_Hof[i];
        wchar_t name[NAME_LEN + 1] = { 0 }; ConvertNameToWide(f.name, name, NAME_LEN + 1);
        wchar_t buf[48];
        if (i & 1)
            RenderUiRect(lx, ly - 2, lw, 18, 1.0f, 1.0f, 1.0f, 0.04f);
        std::swprintf(buf, 48, L"S%u", (unsigned)f.season);
        RenderTextEx(lx + 6, ly, 60, buf, 196, 178, 142, 255, RT3_SORT_LEFT);
        std::swprintf(buf, 48, L"#%u", (unsigned)f.rank);
        RenderTextEx(lx + 80, ly, 50, buf, 238, 210, 142, 255, RT3_SORT_LEFT);
        RenderTextEx(lx + 140, ly, 170, name, 232, 218, 190, 255, RT3_SORT_LEFT);
        RenderTextEx(lx + 320, ly, 40, GetClassName(f.classNumber), 196, 178, 142, 255, RT3_SORT_LEFT);
        std::swprintf(buf, 48, L"%u", f.rating);
        RenderTextEx(lx + 366, ly, 50, buf, 238, 199, 86, 255, RT3_SORT_LEFT);
        ly += 18;
    }
}

bool CNewUIDuelLadder::Render()
{
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, 1.f);
    DWORD oldTextColor = g_pRenderText->GetTextColor();
    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);

    const int x = m_Pos.x, y = m_Pos.y;

    // Panel — deep navy body under the gold frame (matches the navy/gold target palette).
    RenderUiRect(x + 3, y + 3, WINDOW_WIDTH - 6, WINDOW_HEIGHT - 6, 0.055f, 0.075f, 0.130f, 1.0f);
    RenderFrame(x, y, WINDOW_WIDTH, WINDOW_HEIGHT, 14);

    // Title bar — centred title + subtitle. The season number is only shown once the server has
    // reported the real active season (m_ActiveSeason > 0); never a hardcoded guess.
    RenderUiRect(x + 8, y + 8, WINDOW_WIDTH - 16, kTitleH - 14, 0.035f, 0.050f, 0.092f, 1.0f);
    RenderUiRect(x + 8, y + 11, WINDOW_WIDTH - 16, 1, 0.50f, 0.42f, 0.24f, 0.60f);          // gold top accent
    RenderUiRect(x + 8, y + kTitleH - 6, WINDOW_WIDTH - 16, 1, 0.46f, 0.40f, 0.24f, 0.85f); // gold bottom rule
    // Larger title, nudged down so it clears the top accent line. Subtitle removed for room.
    EnsureTitleFont();
    g_pRenderText->SetFont(m_hTitleFont != NULL ? m_hTitleFont : g_hFontBold);
    RenderTextEx(x + 8, y + 15, WINDOW_WIDTH - 16, L"D U E L   L A D D E R", 246, 216, 138, 255, RT3_SORT_CENTER);
    g_pRenderText->SetFont(g_hFont);

    RenderCloseButton();

    // Tabs.
    RenderTabBar();

    switch (m_CurrentTab)
    {
    case TAB_RANKINGS:  RenderRankingsTab(); break;
    case TAB_PROFILE:   RenderProfileTab(); break;
    case TAB_HISTORY:   RenderHistoryTab(); break;
    case TAB_REWARDS:   RenderRewardsTab(); break;
    case TAB_HOF:       RenderHallOfFameTab(); break;
    default: break;
    }

    g_pRenderText->SetTextColor(oldTextColor);
    g_pRenderText->SetBgColor(0);
    DisableAlphaBlend();
    return true;
}
