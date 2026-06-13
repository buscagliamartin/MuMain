#pragma once

#include "UI/NewUI/NewUIBase.h"
#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/Widgets/NewUIButton.h"

class CUITextInputBox; // native Win32 edit control (used for the SEARCH PLAYER text field)

namespace SEASON3B
{
    // BarnaMu: large PvP Duel Ladder hub, opened with the L hotkey.
    //
    // Native MU/NewUI visual style (warm brown/gold, matching the Jewel Bank): code-rendered
    // from the stock 9-slice frame slices + RenderColor fills + flat-warm primitive buttons.
    // No custom art, no static background screenshot, no baked text/rows. Every tab, filter,
    // table row, pagination and action control is a real CNewUIButton hitbox.
    //
    // Five tabs: RANKINGS, MY PROFILE, MATCH HISTORY, SEASON REWARDS, HALL OF FAME.
    // Rankings is a 3-column layout (filters | table | player-details) over a bottom strip
    // (my-status | waiting-to-fight). Existing op 0 (rankings) / op 1 (profile) contracts are
    // unchanged; the new pages/features use new, explicit Duel Ladder ops (see SendRequest).
    class CNewUIDuelLadder : public CNewUIObj
    {
    public:
        CNewUIDuelLadder();
        ~CNewUIDuelLadder();

        bool Create(CNewUIManager* pNewUIMng, int x, int y);
        void Release();

        bool Render();
        bool Update();
        bool UpdateMouseEvent();
        bool UpdateKeyEvent();

        float GetLayerDepth();
        float GetKeyEventOrder();

    public:
        void Toggle();

        // Existing contracts (unchanged).
        void SetTopData(BYTE bracket, BYTE count, const BYTE* data, int dataLen);
        void SetTopGuilds(BYTE count, const BYTE* data, int dataLen); // op 5: guild name per top row
        void SetProfileData(BYTE bracket, BYTE tier, unsigned int rating, unsigned int wins, unsigned int losses, unsigned short rankInBracket);

        // New page data (filled when the matching server ops reply; safe no-ops until then).
        void SetWaitingData(BYTE selfListed, BYTE count, const BYTE* data, int dataLen);
        void SetHistoryData(BYTE count, const BYTE* data, int dataLen);
        void SetHallOfFameData(BYTE count, const BYTE* data, int dataLen);

        // Large hub geometry (reference 640x480 space; centred by NewUISystem).
        static constexpr int WINDOW_WIDTH = 618;
        static constexpr int WINDOW_HEIGHT = 396;

        static constexpr int NAME_LEN = 10;
        static constexpr int MAX_ENTRIES = 100;   // ranking rows held client-side
        static constexpr int VISIBLE_ROWS = 10;   // ranking rows per page
        static constexpr int MAX_WAITING = 32;
        static constexpr int WAITING_VISIBLE = 3;
        static constexpr int MAX_HISTORY = 50;
        static constexpr int HISTORY_VISIBLE = 12;
        static constexpr int MAX_HOF = 50;
        static constexpr int HOF_VISIBLE = 12;
        static constexpr int ENTRY_BYTES = NAME_LEN + 1 + 4 + 4 + 4; // name+class+rating+wins+losses (op 0)

        // Duel Ladder request ops. 0/1 are the existing rankings/profile contracts; the rest are
        // new, explicit ops added for the hub (NOT overloaded onto the ranking packet).
        enum DUEL_OP
        {
            DUEL_OP_RANKINGS = 0,
            DUEL_OP_PROFILE = 1,
            DUEL_OP_WAITING_LIST = 2,
            DUEL_OP_LIST_ME = 3,
            DUEL_OP_REMOVE_ME = 4,
            DUEL_OP_CHALLENGE = 5,
            DUEL_OP_ACCEPT = 6,
            DUEL_OP_DECLINE = 7,
            DUEL_OP_HISTORY = 8,
            DUEL_OP_REWARDS = 9,
            DUEL_OP_HALLOFFAME = 10,
            DUEL_OP_CHALLENGE_RANK = 11, // challenge a ranking row; arg = (bracket << 5) | rowIndex
        };

        enum TAB
        {
            TAB_RANKINGS = 0,
            TAB_PROFILE,
            TAB_HISTORY,
            TAB_REWARDS,
            TAB_HOF,
            TAB_COUNT
        };

    private:
        enum IMAGE_LIST
        {
            IMAGE_BASE_WINDOW_BTN_EXIT = BITMAP_INTERFACE_NEW_PERSONALINVENTORY_BEGIN + 17,

            // Stock 9-slice frame slices (newui_item_table0*), the proven MuHelper/JewelBank
            // pattern — always ship, scale correctly at any size. Kept far from the END-1/END-2
            // custom slots used by other BarnaMu windows.
            IMAGE_DUEL_FRAME_TL = BITMAP_EFFECT_TEXTURE_END - 40,
            IMAGE_DUEL_FRAME_TR,
            IMAGE_DUEL_FRAME_BL,
            IMAGE_DUEL_FRAME_BR,
            IMAGE_DUEL_FRAME_TOP,
            IMAGE_DUEL_FRAME_BOTTOM,
            IMAGE_DUEL_FRAME_LEFT,
            IMAGE_DUEL_FRAME_RIGHT,
        };

        struct Entry
        {
            char name[NAME_LEN + 1];
            BYTE classNumber;
            unsigned int rating;
            unsigned int wins;
            unsigned int losses;
            int streak;                 // optional; 0 until the server sends it
            char guild[NAME_LEN + 1];   // optional; empty until the server sends it
        };

        struct WaitingEntry
        {
            char name[NAME_LEN + 1];
            BYTE classNumber;
            unsigned int rating;
            BYTE tier;
            BYTE bracket;
            unsigned int waitSeconds;
        };

        struct HistoryEntry
        {
            char opponent[NAME_LEN + 1];
            BYTE result;        // 0 = loss, 1 = win
            BYTE myScore;
            BYTE oppScore;
            int ratingChange;
            BYTE bracket;
            unsigned int when;  // unix-ish; rendered relative
        };

        struct HofEntry
        {
            BYTE season;
            BYTE bracket;
            BYTE rank;
            char name[NAME_LEN + 1];
            BYTE classNumber;
            unsigned int rating;
            unsigned int wins;
            unsigned int losses;
        };

        void SetPos(int x, int y);
        void InitButtons();
        void LoadImages();
        void UnloadImages();
        void SendRequest(BYTE op, BYTE arg);

        // SEARCH PLAYER text field (native Win32 edit, focus-managed so typing can't leak to hotkeys).
        void EnsureSearchInput();   // lazily create the edit control (needs g_hWnd)
        void UpdateSearchInput();   // position / show / sync the text each frame on the Rankings tab
        void HideSearchInput();     // hide + release focus (closed, or off the Rankings tab)

        // GUILD text field (same pattern as the search field).
        void EnsureGuildInput();
        void UpdateGuildInput();
        void HideGuildInput();

        void EnsureTitleFont();     // lazily build a larger bold title font from the stock UI font

        // Shared visual helpers (warm/native).
        void RenderFrame(int x, int y, int width, int height, int corner = 14);
        void RenderSectionTitle(int x, int y, int width, const wchar_t* title);
        void RenderCard(int x, int y, int width, int height, float fillAlpha = 0.0f);
        void RenderButtonVisual(CNewUIButton& button, const wchar_t* text, bool active, bool enabled, bool accentGreen = false);
        void RenderCloseButton();
        void RenderTabBar();
        void RenderPendingNotice(int x, int y, int width, int height, const wchar_t* line1, const wchar_t* line2);

        // Page renderers (one per tab).
        void RenderRankingsTab();
        void RenderProfileTab();
        void RenderHistoryTab();
        void RenderRewardsTab();
        void RenderHallOfFameTab();

        // Rankings-tab sub-panels.
        void RenderFiltersPanel(int x, int y, int width, int height);
        void RenderRankingTable(int x, int y, int width, int height);
        void RenderPlayerDetails(int x, int y, int width, int height);
        void RenderMyStatusPanel(int x, int y, int width, int height);
        void RenderWaitingPanel(int x, int y, int width, int height);

        int PageCount() const;
        bool EntryPassesFilters(const Entry& entry) const;
        int BuildVisibleRows(int* outIndices) const; // fills filtered m_Entries indices, returns count

    private:
        CNewUIManager* m_pNewUIMng;
        POINT m_Pos;

        int m_CurrentTab;
        BYTE m_CurrentBracket;  // 1..5 (kept for the existing op 0 contract)
        BYTE m_HofBracket;      // 1..5 selected Hall of Fame bracket sub-tab
        int m_Page;
        int m_SortMode;
        int m_ActiveSeason;     // real active season number (0 = unknown until the server reports it)

        int m_EntryCount;
        Entry m_Entries[MAX_ENTRIES];

        BYTE m_ProfileBracket;
        BYTE m_ProfileTier;
        unsigned int m_ProfileRating;
        unsigned int m_ProfileWins;
        unsigned int m_ProfileLosses;
        unsigned short m_ProfileRank;

        int m_HoveredRow;
        int m_SelectedRow;

        // Client-side rankings filters (applied to the loaded top list).
        unsigned int m_ClassFilter; // bit per class button (0 = show all)
        unsigned int m_TierFilter;  // bit per tier button (0 = show all)
        int m_RatingMinSel;         // 0 = Any, 1..4 = rating min threshold button index
        int m_WinRateMinSel;        // 0 = Any, 1..4 = win-rate min threshold button index
        CUITextInputBox* m_pSearchInput;        // SEARCH PLAYER edit control (created lazily)
        bool m_SearchInputShown;                // whether the edit control is currently shown
        wchar_t m_SearchText[NAME_LEN + 1];     // cached search string (name substring filter)
        CUITextInputBox* m_pGuildInput;         // GUILD edit control (created lazily)
        bool m_GuildInputShown;                 // whether the guild edit control is currently shown
        wchar_t m_GuildText[NAME_LEN + 1];      // cached guild string (guild substring filter)
        HFONT m_hTitleFont;                     // larger bold title font (derived from g_hFontBold)

        bool m_IsWaiting;
        int m_WaitingCount;
        int m_WaitingScroll;    // first visible waiting row (so a long list can be scrolled)
        WaitingEntry m_Waiting[MAX_WAITING];

        int m_HistoryCount;
        HistoryEntry m_History[MAX_HISTORY];

        int m_HofCount;
        HofEntry m_Hof[MAX_HOF];

        // Top controls.
        CNewUIButton m_BtnTab[TAB_COUNT];
        CNewUIButton m_BtnClose;
        CNewUIButton m_BtnHelp;
        CNewUIButton m_BtnMin;

        // Filter controls (rendered as real controls; behaviour pending server support).
        CNewUIButton m_BtnSeason;
        CNewUIButton m_BtnBracket[3];   // 1v1 / Best of 3 / Best of 5
        CNewUIButton m_BtnClass[7];     // DK DW ELF SUM MG DL RF
        CNewUIButton m_BtnTier[6];      // Bronze..Master
        CNewUIButton m_BtnRating[5];    // Any / 1.4k / 1.6k / 1.8k / 2.0k (min rating)
        CNewUIButton m_BtnWinRate[5];   // Any / 50% / 60% / 70% / 80% (min win rate)
        CNewUIButton m_BtnSearchPlayer;
        CNewUIButton m_BtnSearchGuild;
        CNewUIButton m_BtnResetFilters;
        CNewUIButton m_BtnApplyFilters;

        // Pagination + sort.
        CNewUIButton m_BtnFirst;
        CNewUIButton m_BtnPrev;
        CNewUIButton m_BtnNext;
        CNewUIButton m_BtnLast;
        CNewUIButton m_BtnSort;

        // Player-details actions.
        CNewUIButton m_BtnChallenge;
        CNewUIButton m_BtnViewGear;
        CNewUIButton m_BtnCompare;

        // Waiting-to-fight.
        CNewUIButton m_BtnListMe;
        CNewUIButton m_BtnRemoveMe;
        CNewUIButton m_BtnRefreshWaiting;
        CNewUIButton m_BtnWaitUp;       // scroll the waiting list up a page
        CNewUIButton m_BtnWaitDown;     // scroll the waiting list down a page
        CNewUIButton m_BtnWaitingChallenge[WAITING_VISIBLE];

        // Hall of Fame per-bracket sub-tabs (T1..T5).
        CNewUIButton m_BtnHofBracket[5];
    };
}
