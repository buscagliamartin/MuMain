#pragma once

#include "UI/NewUI/NewUIBase.h"
#include "UI/NewUI/NewUIManager.h"
#include "UI/NewUI/NewUI3DRenderMng.h"
#include "UI/NewUI/Widgets/NewUIButton.h"

namespace SEASON3B
{
    // BarnaMu: standalone mailbox window.
    // Uses the existing Auction House mailbox packet flow (0xBF / 0x31, view 2),
    // but keeps the UI separated from Auction House so it can later become a
    // generic delivery inbox for WCoin purchases, rewards, compensation, etc.
    class CNewUIMailbox : public CNewUIObj, public INewUI3DRenderObj
    {
    public:
        struct EntryView
        {
            unsigned int EntryNumber;
            unsigned int Amount;
            unsigned short ItemType;
            BYTE ItemLevel;
            BYTE Currency;
            BYTE JewelSlot;
            BYTE Status;
            wchar_t ItemName[48];
            wchar_t SourceName[12];
            wchar_t ItemSummary[256];
            BYTE ItemDataLength;
            BYTE ItemData[15];
        };

        CNewUIMailbox();
        ~CNewUIMailbox();

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
        void SetMailboxHeader(BYTE view, BYTE page, BYTE count);
        void AddMailboxEntry(const EntryView& entry);
        void SetStatusMessage(const wchar_t* message);

    public:
        static constexpr int WINDOW_WIDTH = 520;
        static constexpr int WINDOW_HEIGHT = 430;
        static constexpr int MAX_ROWS = 6;

    private:
        static constexpr int HEADER_HEIGHT = 26;
        static constexpr int TABLE_X = 14;
        static constexpr int TABLE_Y = 94;
        static constexpr int TABLE_WIDTH = 292;
        static constexpr int TABLE_ROW_HEIGHT = 48;
        static constexpr int DETAILS_X = 320;
        static constexpr int DETAILS_Y = 58;
        static constexpr int DETAILS_WIDTH = 186;
        static constexpr int DETAILS_HEIGHT = 332;
        static constexpr int FOOTER_Y = 394;
        static constexpr int FOOTER_HEIGHT = 28;

        void SetPos(int x, int y);
        void InitButtons();
        void SendRequest(BYTE op, BYTE arg1, BYTE currency, BYTE jewelSlot, unsigned int arg2, unsigned int arg3);
        void RequestMailbox();
        bool ProcessMouseButtons();
        bool SendClaimSelected();
        bool SendClaimAll();
        void SendClaimEntry(const EntryView& entry);
        bool IsLikelyPayout(const EntryView& entry) const;

        void RenderBack();
        void RenderPanel(int x, int y, int width, int height, const wchar_t* title);
        void RenderTable();
        void RenderDetails();
        void RenderFooter();
        void RenderFlatButton(CNewUIButton& button, const wchar_t* text, bool enabled, int tone);
        const wchar_t* GetCurrencyText(BYTE currency, BYTE jewelSlot) const;
        const wchar_t* GetStatusText(BYTE status) const;
        const wchar_t* GetEntryTypeText(const EntryView& entry) const;

    private:
        CNewUIManager* m_pNewUIMng;
        CNewUI3DRenderMng* m_pNewUI3DRenderMng;
        POINT m_Pos;
        BYTE m_CurrentPage;
        int m_SelectedRow;
        int m_HoveredRow;
        int m_RowCount;
        EntryView m_Entries[MAX_ROWS];
        wchar_t m_StatusMessage[128];

        CNewUIButton m_BtnClose;
        CNewUIButton m_BtnRefresh;
        CNewUIButton m_BtnClaimSelected;
        CNewUIButton m_BtnClaimAll;
    };
}
