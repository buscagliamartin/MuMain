#include "stdafx.h"
#include "stdafx.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <span>

#include "UI/NewUI/MailboxWindow.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Inventory/NewUIItemMng.h"
#include "Engine/Object/ZzzInventory.h"
#include "Dotnet/Connection.h"

extern Connection* SocketClient;

using namespace SEASON3B;

namespace
{
    constexpr BYTE MAILBOX_VIEW = 2;

    const wchar_t* const s_MailboxJewelNames[17] =
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

    void RenderMailboxRect(int x, int y, int width, int height, float red, float green, float blue, float alpha)
    {
        if (width <= 0 || height <= 0 || alpha <= 0.f)
            return;

        glColor4f(red, green, blue, alpha);
        glDisable(GL_TEXTURE_2D);
        RenderColor(float(x), float(y), float(width), float(height));
        EndRenderColor();
        glColor4f(1.f, 1.f, 1.f, 1.f);
    }

    void RenderMailboxLine(int x, int y, int width, int height, float red, float green, float blue, float alpha)
    {
        RenderMailboxRect(x, y, width, height, red, green, blue, alpha);
    }

    const wchar_t* StripMailboxPrefix(const wchar_t* text)
    {
        if (text == NULL)
        {
            return L"";
        }

        if (std::wcsncmp(text, L"Payout: ", 8) == 0)
        {
            text += 8;
        }
        else if (std::wcsncmp(text, L"Returned: ", 10) == 0)
        {
            text += 10;
        }

        // Older mailbox entries can contain the inventory-slot prefix from item.ToString(),
        // e.g. "Slot 16: Excellent ...". It is useful for debugging but noisy in the UI.
        if (std::wcsncmp(text, L"Slot ", 5) == 0)
        {
            const wchar_t* cursor = text + 5;
            while (*cursor >= L'0' && *cursor <= L'9')
            {
                ++cursor;
            }

            if (cursor[0] == L':' && cursor[1] == L' ')
            {
                return cursor + 2;
            }
        }

        return text;
    }

    const wchar_t* NormalizeMailboxSourceName(const wchar_t* text)
    {
        if (text == NULL || text[0] == L'\0')
        {
            return L"Auction";
        }

        // Server row sender field is currently short enough to truncate "Auction House".
        // Normalize the known truncated value client-side until the packet field is widened.
        if (std::wcsncmp(text, L"Auction Hou", 11) == 0 || std::wcscmp(text, L"Auction") == 0)
        {
            return L"Auction House";
        }

        return text;
    }

    bool MailboxTextContains(const wchar_t* text, const wchar_t* token)
    {
        return text != NULL && token != NULL && std::wcsstr(text, token) != NULL;
    }

    bool IsMailboxExcellentEntry(const CNewUIMailbox::EntryView& entry)
    {
        return MailboxTextContains(entry.ItemName, L"Excellent") || MailboxTextContains(entry.ItemSummary, L"Excellent");
    }

    bool IsMailboxLargeIconItem(unsigned short itemType)
    {
        const int group = itemType / 512;
        return group == 12 || group == 13;
    }

    void GetMailboxItemRenderSize(unsigned short itemType, bool detailView, float& width, float& height)
    {
        const int group = itemType / 512;
        if (detailView)
        {
            if (group >= 0 && group <= 5)
            {
                width = 28.f;
                height = 42.f;
            }
            else if (group == 12 || group == 13)
            {
                width = 52.f;
                height = 44.f;
            }
            else
            {
                width = 42.f;
                height = 42.f;
            }
        }
        else
        {
            if (group >= 0 && group <= 5)
            {
                width = 8.f;
                height = 16.f;
            }
            else if (group == 12 || group == 13)
            {
                width = 9.f;
                height = 8.f;
            }
            else
            {
                width = 16.f;
                height = 16.f;
            }
        }
    }

    ITEM* CreateMailboxPreviewItem(const CNewUIMailbox::EntryView& entry)
    {
        if (g_pNewItemMng == NULL || entry.ItemDataLength < 5 || entry.ItemDataLength > 15)
        {
            return NULL;
        }

        return g_pNewItemMng->CreateItem(std::span<const BYTE>(entry.ItemData, entry.ItemDataLength));
    }

    void RenderMailboxSummaryLines(int x, int y, int width, const wchar_t* summary, int maxLines, bool centered)
    {
        if (summary == NULL || summary[0] == L'\0' || maxLines <= 0)
        {
            return;
        }

        const int maxCharsPerLine = std::max(10, std::min(95, width / 4));
        const wchar_t* cursor = summary;
        int line = 0;
        while (cursor != NULL && *cursor != L'\0' && line < maxLines)
        {
            const wchar_t* separator = std::wcsstr(cursor, L" | ");
            const int partLength = static_cast<int>(separator != NULL ? separator - cursor : std::wcslen(cursor));
            int offset = 0;
            while (offset < partLength && line < maxLines)
            {
                while (offset < partLength && std::iswspace(cursor[offset]) != 0)
                    ++offset;

                if (offset >= partLength)
                    break;

                int length = std::min(partLength - offset, maxCharsPerLine);
                if (offset + length < partLength)
                {
                    for (int candidate = length; candidate > maxCharsPerLine / 2; --candidate)
                    {
                        if (std::iswspace(cursor[offset + candidate]) != 0)
                        {
                            length = candidate;
                            break;
                        }
                    }
                }

                while (length > 0 && std::iswspace(cursor[offset + length - 1]) != 0)
                    --length;

                if (length <= 0)
                    break;

                wchar_t part[96] = { 0 };
                const int copyLength = std::min(length, 95);
                wcsncpy(part, cursor + offset, copyLength);
                part[copyLength] = L'\0';

                if (MailboxTextContains(part, L"Excellent") || MailboxTextContains(part, L"Ancient"))
                    g_pRenderText->SetTextColor(86, 236, 86, 255);
                else if (MailboxTextContains(part, L"Luck") || MailboxTextContains(part, L"Skill"))
                    g_pRenderText->SetTextColor(100, 180, 255, 255);
                else
                    g_pRenderText->SetTextColor(line == 0 ? 236 : 100, line == 0 ? 202 : 180, line == 0 ? 104 : 255, 255);

                g_pRenderText->RenderText(x, y + line * 12, part, width, 0, centered ? RT3_SORT_CENTER : RT3_SORT_LEFT);
                ++line;
                offset += length;
            }

            cursor = separator != NULL ? separator + 3 : NULL;
        }
    }
}

CNewUIMailbox::CNewUIMailbox()
{
    m_pNewUIMng = NULL;
    m_pNewUI3DRenderMng = NULL;
    m_Pos.x = 0;
    m_Pos.y = 0;
    m_CurrentPage = 1;
    m_SelectedRow = -1;
    m_HoveredRow = -1;
    m_RowCount = 0;
    m_StatusMessage[0] = L'\0';
    ZeroMemory(m_Entries, sizeof(m_Entries));
}

CNewUIMailbox::~CNewUIMailbox()
{
    Release();
}

bool CNewUIMailbox::Create(CNewUIManager* pNewUIMng, CNewUI3DRenderMng* pNewUI3DRenderMng, int x, int y)
{
    if (NULL == pNewUIMng || NULL == pNewUI3DRenderMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(INTERFACE_MAILBOX, this);

    m_pNewUI3DRenderMng = pNewUI3DRenderMng;
    m_pNewUI3DRenderMng->Add3DRenderObj(this, INFORMATION_CAMERA_Z_ORDER);

    SetPos(x, y);
    InitButtons();
    Show(false);

    return true;
}

void CNewUIMailbox::Release()
{
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

void CNewUIMailbox::SetPos(int x, int y)
{
    m_Pos.x = x;
    m_Pos.y = y;
}

void CNewUIMailbox::InitButtons()
{
    m_BtnClose.ChangeButtonInfo(m_Pos.x + WINDOW_WIDTH - 42, m_Pos.y + 4, 36, 29);
    m_BtnClose.ChangeText(L"");
    m_BtnClose.ChangeToolTipText(L"Close", TRUE);

    m_BtnRefresh.ChangeButtonInfo(m_Pos.x + 14, m_Pos.y + FOOTER_Y + 5, 72, 18);
    m_BtnRefresh.ChangeText(L"");
    m_BtnRefresh.ChangeToolTipText(L"Refresh mailbox", TRUE);

    m_BtnClaimSelected.ChangeButtonInfo(m_Pos.x + WINDOW_WIDTH - 178, m_Pos.y + FOOTER_Y + 4, 74, 20);
    m_BtnClaimSelected.ChangeText(L"");
    m_BtnClaimSelected.ChangeToolTipText(L"Claim selected mailbox entry", TRUE);

    m_BtnClaimAll.ChangeButtonInfo(m_Pos.x + WINDOW_WIDTH - 98, m_Pos.y + FOOTER_Y + 4, 84, 20);
    m_BtnClaimAll.ChangeText(L"");
    m_BtnClaimAll.ChangeToolTipText(L"Claim all visible mailbox entries", TRUE);
}

float CNewUIMailbox::GetLayerDepth()
{
    return 3.35f;
}

float CNewUIMailbox::GetKeyEventOrder()
{
    return 3.35f;
}

void CNewUIMailbox::Toggle()
{
    if (IsVisible())
    {
        Show(false);
        return;
    }

    Show(true);
    RequestMailbox();
}

void CNewUIMailbox::SetMailboxHeader(BYTE, BYTE page, BYTE)
{
    m_CurrentPage = page == 0 ? 1 : page;
    m_SelectedRow = -1;
    m_HoveredRow = -1;
    m_RowCount = 0;
    ZeroMemory(m_Entries, sizeof(m_Entries));
    SetStatusMessage(L"Mailbox refreshed.");
}

void CNewUIMailbox::AddMailboxEntry(const EntryView& entry)
{
    if (m_RowCount >= MAX_ROWS)
        return;

    m_Entries[m_RowCount++] = entry;
}

void CNewUIMailbox::SetStatusMessage(const wchar_t* message)
{
    if (message == NULL)
        message = L"";

    wcsncpy_s(m_StatusMessage, _countof(m_StatusMessage), message, _TRUNCATE);
}

void CNewUIMailbox::SendRequest(BYTE op, BYTE arg1, BYTE currency, BYTE jewelSlot, unsigned int arg2, unsigned int arg3)
{
    if (SocketClient == NULL)
        return;

    SocketClient->ToGameServer()->SendAuctionHouseRequest(op, arg1, currency, jewelSlot, arg2, arg3);
}

void CNewUIMailbox::RequestMailbox()
{
    SetStatusMessage(L"Requesting mailbox...");
    SendRequest(5, m_CurrentPage, 0, 0xFF, 0, 0);
}

bool CNewUIMailbox::ProcessMouseButtons()
{
    if (m_BtnClose.UpdateMouseEvent())
    {
        g_pNewUISystem->Hide(INTERFACE_MAILBOX);
        return true;
    }

    if (m_BtnRefresh.UpdateMouseEvent())
    {
        RequestMailbox();
        return true;
    }

    if (m_BtnClaimSelected.UpdateMouseEvent())
        return SendClaimSelected();

    if (m_BtnClaimAll.UpdateMouseEvent())
        return SendClaimAll();

    return false;
}

bool CNewUIMailbox::SendClaimSelected()
{
    if (m_SelectedRow < 0 || m_SelectedRow >= m_RowCount)
    {
        SetStatusMessage(L"No mailbox entry selected.");
        return true;
    }

    const EntryView& entry = m_Entries[m_SelectedRow];
    if (entry.EntryNumber == 0)
    {
        SetStatusMessage(L"Invalid mailbox entry.");
        return true;
    }

    SetStatusMessage(L"Claiming entry...");
    SendClaimEntry(entry);

    return true;
}

bool CNewUIMailbox::SendClaimAll()
{
    if (m_RowCount <= 0)
    {
        SetStatusMessage(L"No mailbox entries to claim.");
        return true;
    }

    int sent = 0;
    for (int i = 0; i < m_RowCount; ++i)
    {
        const EntryView& entry = m_Entries[i];
        if (entry.EntryNumber == 0)
            continue;

        SendClaimEntry(entry);
        ++sent;
    }

    wchar_t message[64] = { 0 };
    if (sent == 1)
    {
        std::swprintf(message, 64, L"Claiming 1 entry...");
    }
    else
    {
        std::swprintf(message, 64, L"Claiming %d entries...", sent);
    }

    SetStatusMessage(message);
    return true;
}

void CNewUIMailbox::SendClaimEntry(const EntryView& entry)
{
    SendRequest(IsLikelyPayout(entry) ? 8 : 6, 0, 0, 0xFF, entry.EntryNumber, 0);
}

bool CNewUIMailbox::IsLikelyPayout(const EntryView& entry) const
{
    if (entry.ItemName[0] == L'\0')
        return true;

    // Payout mailbox rows are expected to not carry a meaningful item graph.
    // Keep this heuristic non-authoritative; the server still validates op 6/op 8.
    return entry.ItemType == 0 && entry.ItemLevel == 0 && entry.Amount > 0;
}

bool CNewUIMailbox::Update()
{
    return true;
}

bool CNewUIMailbox::UpdateMouseEvent()
{
    if (!CheckMouseIn(m_Pos.x, m_Pos.y, WINDOW_WIDTH, WINDOW_HEIGHT))
        return true;

    m_HoveredRow = -1;

    if (ProcessMouseButtons())
        return false;

    const int tableX = m_Pos.x + TABLE_X;
    const int tableY = m_Pos.y + TABLE_Y;
    for (int i = 0; i < m_RowCount; ++i)
    {
        const int rowY = tableY + i * TABLE_ROW_HEIGHT;
        if (CheckMouseIn(tableX, rowY, TABLE_WIDTH, TABLE_ROW_HEIGHT))
        {
            m_HoveredRow = i;
            if (IsRelease(VK_LBUTTON))
            {
                m_SelectedRow = i;
                return false;
            }
        }
    }

    return false;
}

bool CNewUIMailbox::UpdateKeyEvent()
{
    if (IsVisible() && IsPress(VK_ESCAPE) == true)
    {
        g_pNewUISystem->Hide(INTERFACE_MAILBOX);
        return false;
    }

    return true;
}

bool CNewUIMailbox::IsVisible() const
{
    return CNewUIObj::IsVisible();
}

void CNewUIMailbox::RenderBack()
{
    RenderMailboxRect(m_Pos.x, m_Pos.y, WINDOW_WIDTH, WINDOW_HEIGHT, 0.010f, 0.012f, 0.015f, 0.96f);
    RenderMailboxRect(m_Pos.x + 2, m_Pos.y + 2, WINDOW_WIDTH - 4, WINDOW_HEIGHT - 4, 0.035f, 0.040f, 0.048f, 0.94f);
    RenderMailboxRect(m_Pos.x + 6, m_Pos.y + 6, WINDOW_WIDTH - 12, HEADER_HEIGHT, 0.060f, 0.064f, 0.072f, 0.94f);
    RenderMailboxLine(m_Pos.x + 6, m_Pos.y + 33, WINDOW_WIDTH - 12, 1, 0.38f, 0.34f, 0.24f, 0.62f);

    g_pRenderText->SetFont(g_hFontBold);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(238, 210, 142, 255);
    g_pRenderText->RenderText(m_Pos.x, m_Pos.y + 14, L"MAILBOX", WINDOW_WIDTH, 0, RT3_SORT_CENTER);
}

void CNewUIMailbox::RenderPanel(int x, int y, int width, int height, const wchar_t* title)
{
    RenderMailboxRect(x, y, width, height, 0.015f, 0.017f, 0.020f, 0.92f);
    RenderMailboxRect(x + 1, y + 1, width - 2, height - 2, 0.055f, 0.065f, 0.078f, 0.88f);
    RenderMailboxRect(x + 3, y + 3, width - 6, 18, 0.095f, 0.100f, 0.110f, 0.92f);
    RenderMailboxLine(x, y, width, 1, 0.46f, 0.39f, 0.24f, 0.70f);
    RenderMailboxLine(x, y + height - 1, width, 1, 0.07f, 0.08f, 0.09f, 0.92f);
    RenderMailboxLine(x, y, 1, height, 0.35f, 0.34f, 0.31f, 0.68f);
    RenderMailboxLine(x + width - 1, y, 1, height, 0.06f, 0.07f, 0.08f, 0.90f);

    if (title != NULL)
    {
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(226, 190, 112, 255);
        g_pRenderText->RenderText(x + 6, y + 6, title, width - 12, 0, RT3_SORT_CENTER);
    }
}

void CNewUIMailbox::RenderTable()
{
    const int x = m_Pos.x + TABLE_X;
    const int y = m_Pos.y + TABLE_Y;

    RenderPanel(x - 6, y - 36, TABLE_WIDTH + 12, DETAILS_HEIGHT, L"MAILBOX ENTRIES");

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(156, 184, 218, 255);
    g_pRenderText->RenderText(x + 8, y - 13, L"DELIVERIES", TABLE_WIDTH - 16, 0, RT3_SORT_LEFT);

    wchar_t pageText[32] = { 0 };
    std::swprintf(pageText, 32, L"Page %u", m_CurrentPage);
    g_pRenderText->SetTextColor(196, 177, 125, 255);
    g_pRenderText->RenderText(x + TABLE_WIDTH - 72, y - 13, pageText, 64, 0, RT3_SORT_RIGHT);

    g_pRenderText->SetTextColor(184, 188, 194, 255);
    g_pRenderText->RenderText(x + 48, y - 1, L"Item", 116, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 178, y - 1, L"From", 42, 0, RT3_SORT_LEFT);
    g_pRenderText->RenderText(x + 222, y - 1, L"Value", 62, 0, RT3_SORT_RIGHT);

    for (int i = 0; i < MAX_ROWS; ++i)
    {
        const int rowY = y + i * TABLE_ROW_HEIGHT;
        const bool alt = (i % 2) != 0;
        RenderMailboxRect(x, rowY, TABLE_WIDTH, TABLE_ROW_HEIGHT - 4, 0.012f, 0.014f, 0.018f, 0.92f);
        RenderMailboxRect(x + 1, rowY + 1, TABLE_WIDTH - 2, TABLE_ROW_HEIGHT - 6,
            alt ? 0.038f : 0.050f, alt ? 0.044f : 0.054f, alt ? 0.052f : 0.064f, 0.86f);
        RenderMailboxLine(x + 1, rowY + 1, TABLE_WIDTH - 2, 1, 0.38f, 0.32f, 0.20f, 0.44f);
    }

    for (int i = 0; i < m_RowCount; ++i)
    {
        const int rowY = y + i * TABLE_ROW_HEIGHT;
        if (i == m_HoveredRow)
        {
            RenderMailboxRect(x + 1, rowY + 1, TABLE_WIDTH - 2, TABLE_ROW_HEIGHT - 6, 0.06f, 0.15f, 0.24f, 0.44f);
        }

        if (i == m_SelectedRow)
        {
            RenderMailboxRect(x + 1, rowY + 1, TABLE_WIDTH - 2, TABLE_ROW_HEIGHT - 6, 0.02f, 0.20f, 0.38f, 0.62f);
            RenderMailboxLine(x + 1, rowY + 1, TABLE_WIDTH - 2, 1, 0.14f, 0.58f, 1.00f, 0.92f);
            RenderMailboxLine(x + 1, rowY + TABLE_ROW_HEIGHT - 6, TABLE_WIDTH - 2, 1, 0.92f, 0.62f, 0.12f, 0.70f);
        }
    }

    g_pRenderText->SetFont(g_hFont);
    for (int i = 0; i < m_RowCount; ++i)
    {
        const EntryView& entry = m_Entries[i];
        const int rowY = y + i * TABLE_ROW_HEIGHT;

        wchar_t amountText[48] = { 0 };
        if (entry.Amount > 0)
            std::swprintf(amountText, 48, L"%u %ls", entry.Amount, GetCurrencyText(entry.Currency, entry.JewelSlot));
        else
            std::swprintf(amountText, 48, L"Returned");

        RenderMailboxRect(x + 4, rowY + 4, 40, 36, 0.006f, 0.007f, 0.010f, 0.96f);
        RenderMailboxRect(x + 5, rowY + 5, 38, 34, 0.028f, 0.033f, 0.040f, 0.92f);
        RenderMailboxLine(x + 5, rowY + 5, 38, 1, 0.48f, 0.40f, 0.24f, 0.44f);

        if (IsLikelyPayout(entry))
        {
            g_pRenderText->SetTextColor(236, 184, 86, 255);
            g_pRenderText->RenderText(x + 5, rowY + 16, L"$", 38, 0, RT3_SORT_CENTER);
        }

        if (IsMailboxExcellentEntry(entry))
            g_pRenderText->SetTextColor(84, 236, 86, 255);
        else
            g_pRenderText->SetTextColor(235, 202, 104, 255);
        g_pRenderText->RenderText(x + 48, rowY + 16, entry.ItemName[0] ? StripMailboxPrefix(entry.ItemName) : L"Mailbox entry", 126, 0, RT3_SORT_LEFT);

        g_pRenderText->SetTextColor(214, 216, 216, 255);
        g_pRenderText->RenderText(x + 178, rowY + 16, NormalizeMailboxSourceName(entry.SourceName), 42, 0, RT3_SORT_LEFT);
        g_pRenderText->SetTextColor(236, 184, 86, 255);
        g_pRenderText->RenderText(x + 222, rowY + 16, amountText, 62, 0, RT3_SORT_RIGHT);
    }

    if (m_RowCount == 0)
    {
        g_pRenderText->SetTextColor(150, 154, 160, 255);
        g_pRenderText->RenderText(x, y + 134, L"No mailbox entries.", TABLE_WIDTH, 0, RT3_SORT_CENTER);
    }
}

void CNewUIMailbox::RenderDetails()
{
    const int x = m_Pos.x + DETAILS_X;
    const int y = m_Pos.y + DETAILS_Y;
    const EntryView* entry = (m_SelectedRow >= 0 && m_SelectedRow < m_RowCount) ? &m_Entries[m_SelectedRow] : NULL;

    RenderPanel(x, y, DETAILS_WIDTH, DETAILS_HEIGHT, L"DETAILS");

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);

    if (entry == NULL)
    {
        g_pRenderText->SetTextColor(150, 154, 160, 255);
        g_pRenderText->RenderText(x + 8, y + 112, L"Select an entry", DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(x + 8, y + 126, L"to inspect it.", DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);
        return;
    }

    wchar_t amountText[48] = { 0 };
    if (entry->Amount > 0)
        std::swprintf(amountText, 48, L"%u %ls", entry->Amount, GetCurrencyText(entry->Currency, entry->JewelSlot));
    else
        std::swprintf(amountText, 48, L"Returned item");

    const wchar_t* cleanName = entry->ItemName[0] ? StripMailboxPrefix(entry->ItemName) : L"Mailbox entry";
    if (IsMailboxExcellentEntry(*entry))
        g_pRenderText->SetTextColor(84, 236, 86, 255);
    else
        g_pRenderText->SetTextColor(235, 202, 104, 255);
    g_pRenderText->RenderText(x + 8, y + 28, cleanName, DETAILS_WIDTH - 16, 0, RT3_SORT_CENTER);

    const int previewBoxWidth = 104;
    const int previewBoxHeight = 82;
    const int previewBoxX = x + (DETAILS_WIDTH - previewBoxWidth) / 2;
    RenderMailboxRect(previewBoxX, y + 45, previewBoxWidth, previewBoxHeight, 0.006f, 0.007f, 0.010f, 0.96f);
    RenderMailboxRect(previewBoxX + 2, y + 47, previewBoxWidth - 4, previewBoxHeight - 4, 0.028f, 0.033f, 0.040f, 0.92f);
    RenderMailboxLine(previewBoxX + 2, y + 47, previewBoxWidth - 4, 1, 0.48f, 0.40f, 0.24f, 0.50f);

    if (IsLikelyPayout(*entry))
    {
        g_pRenderText->SetTextColor(236, 184, 86, 255);
        g_pRenderText->RenderText(previewBoxX + 2, y + 78, L"PAYOUT", previewBoxWidth - 4, 0, RT3_SORT_CENTER);
        g_pRenderText->SetTextColor(216, 218, 218, 255);
        g_pRenderText->RenderText(x + 8, y + 148, L"From", 36, 0, RT3_SORT_LEFT);
        g_pRenderText->RenderText(x + 46, y + 148, NormalizeMailboxSourceName(entry->SourceName), DETAILS_WIDTH - 54, 0, RT3_SORT_LEFT);
        g_pRenderText->SetTextColor(236, 184, 86, 255);
        g_pRenderText->RenderText(x + 8, y + 162, L"Value", 36, 0, RT3_SORT_LEFT);
        g_pRenderText->RenderText(x + 46, y + 162, amountText, DETAILS_WIDTH - 54, 0, RT3_SORT_LEFT);
        return;
    }

    ITEM* previewItem = CreateMailboxPreviewItem(*entry);
    if (previewItem != NULL)
    {
        RenderMailboxRect(x + 8, y + 120, DETAILS_WIDTH - 16, 168, 0.006f, 0.007f, 0.010f, 0.88f);
        RenderMailboxRect(x + 10, y + 122, DETAILS_WIDTH - 20, 156, 0.020f, 0.024f, 0.030f, 0.74f);
        RenderMailboxLine(x + 10, y + 122, DETAILS_WIDTH - 20, 1, 0.46f, 0.38f, 0.23f, 0.50f);
        RenderItemInfo(x + (DETAILS_WIDTH / 2), y + 118, previewItem, false, 0, false);
        g_pNewItemMng->DeleteItem(previewItem);
    }
    else
    {
        RenderMailboxSummaryLines(x + 8, y + 122, DETAILS_WIDTH - 16, entry->ItemSummary, 10, true);
    }
}

void CNewUIMailbox::RenderFooter()
{
    const int x = m_Pos.x + 8;
    const int y = m_Pos.y + FOOTER_Y;
    RenderPanel(x, y, WINDOW_WIDTH - 16, FOOTER_HEIGHT, NULL);

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(236, 184, 86, 255);

    wchar_t countText[32] = { 0 };
    if (m_RowCount == 1)
    {
        std::swprintf(countText, 32, L"1 entry");
    }
    else
    {
        std::swprintf(countText, 32, L"%d entries", m_RowCount);
    }

    g_pRenderText->RenderText(x + 84, y + 8, countText, 78, 0, RT3_SORT_LEFT);

    g_pRenderText->SetTextColor(156, 166, 176, 255);
    g_pRenderText->RenderText(x + 164, y + 8, m_StatusMessage, 116, 0, RT3_SORT_LEFT);
}

void CNewUIMailbox::RenderFlatButton(CNewUIButton& button, const wchar_t* text, bool enabled, int tone)
{
    const POINT& pos = button.GetPos();
    const POINT& size = button.GetSize();
    const BUTTON_STATE state = button.GetBTState();
    const bool hot = enabled && state == BUTTON_STATE_OVER;
    const bool down = enabled && state == BUTTON_STATE_DOWN;

    float red = 0.12f, green = 0.13f, blue = 0.15f;
    if (tone == 1)
    {
        red = 0.05f; green = 0.23f; blue = 0.10f;
    }
    else if (tone == 2)
    {
        red = 0.22f; green = 0.11f; blue = 0.02f;
    }
    else if (!enabled)
    {
        red = 0.06f; green = 0.06f; blue = 0.06f;
    }

    if (hot)
    {
        red += 0.05f; green += 0.05f; blue += 0.06f;
    }

    if (down)
    {
        red *= 0.75f; green *= 0.75f; blue *= 0.75f;
    }

    RenderMailboxRect(pos.x, pos.y, size.x, size.y, 0.010f, 0.012f, 0.014f, enabled ? 0.95f : 0.72f);
    RenderMailboxRect(pos.x + 1, pos.y + 1, size.x - 2, size.y - 2, red, green, blue, enabled ? 0.92f : 0.66f);
    RenderMailboxLine(pos.x + 1, pos.y + 1, size.x - 2, 1, enabled ? 0.60f : 0.20f, enabled ? 0.50f : 0.20f, enabled ? 0.30f : 0.20f, 0.78f);

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetBgColor(0);
    if (!enabled)
        g_pRenderText->SetTextColor(112, 112, 112, 255);
    else if (tone == 1)
        g_pRenderText->SetTextColor(214, 255, 214, 255);
    else if (tone == 2)
        g_pRenderText->SetTextColor(255, 236, 188, 255);
    else
        g_pRenderText->SetTextColor(214, 218, 222, 255);

    g_pRenderText->RenderText(pos.x, pos.y + 6, text, size.x, 0, RT3_SORT_CENTER);
}

void CNewUIMailbox::Render3D()
{
    if (!IsVisible())
        return;

    const int tableX = m_Pos.x + TABLE_X;
    const int tableY = m_Pos.y + TABLE_Y;
    for (int i = 0; i < m_RowCount; ++i)
    {
        if (IsLikelyPayout(m_Entries[i]))
            continue;

        const int rowY = tableY + i * TABLE_ROW_HEIGHT;
        glColor4f(1.f, 1.f, 1.f, 1.f);
        float iconWidth = 18.f;
        float iconHeight = 18.f;
        GetMailboxItemRenderSize(m_Entries[i].ItemType, false, iconWidth, iconHeight);
        BYTE excellentFlags = 0;
        BYTE ancientDiscriminator = 0;
        ITEM* rowPreviewItem = CreateMailboxPreviewItem(m_Entries[i]);
        if (rowPreviewItem != NULL)
        {
            excellentFlags = static_cast<BYTE>(rowPreviewItem->ExcellentFlags);
            ancientDiscriminator = static_cast<BYTE>(rowPreviewItem->AncientDiscriminator);
            g_pNewItemMng->DeleteItem(rowPreviewItem);
        }
        RenderItem3D(
            float(tableX + 4) + (40.f - iconWidth) / 2.f,
            float(rowY + 4) + (36.f - iconHeight) / 2.f,
            iconWidth,
            iconHeight,
            m_Entries[i].ItemType,
            m_Entries[i].ItemLevel,
            excellentFlags,
            ancientDiscriminator,
            false);
    }

    if (m_SelectedRow >= 0 && m_SelectedRow < m_RowCount && !IsLikelyPayout(m_Entries[m_SelectedRow]))
    {
        const EntryView& entry = m_Entries[m_SelectedRow];
        glColor4f(1.f, 1.f, 1.f, 1.f);
        float iconWidth = 48.f;
        float iconHeight = 44.f;
        GetMailboxItemRenderSize(entry.ItemType, true, iconWidth, iconHeight);
        constexpr float previewBoxWidth = 104.f;
        constexpr float previewBoxHeight = 82.f;
        BYTE excellentFlags = 0;
        BYTE ancientDiscriminator = 0;
        ITEM* detailPreviewItem = CreateMailboxPreviewItem(entry);
        if (detailPreviewItem != NULL)
        {
            excellentFlags = static_cast<BYTE>(detailPreviewItem->ExcellentFlags);
            ancientDiscriminator = static_cast<BYTE>(detailPreviewItem->AncientDiscriminator);
            g_pNewItemMng->DeleteItem(detailPreviewItem);
        }
        RenderItem3D(
            float(m_Pos.x + DETAILS_X + (DETAILS_WIDTH - int(previewBoxWidth)) / 2) + (previewBoxWidth - iconWidth) / 2.f,
            float(m_Pos.y + DETAILS_Y + 45) + (previewBoxHeight - iconHeight) / 2.f,
            iconWidth,
            iconHeight,
            entry.ItemType,
            entry.ItemLevel,
            excellentFlags,
            ancientDiscriminator,
            false);
    }
}

bool CNewUIMailbox::Render()
{
    EnableAlphaTest();
    glColor4f(1.f, 1.f, 1.f, 1.f);

    RenderBack();
    RenderTable();
    RenderDetails();
    RenderFooter();

    const bool hasSelection = m_SelectedRow >= 0 && m_SelectedRow < m_RowCount;

    RenderFlatButton(m_BtnRefresh, L"Refresh", true, 0);
    m_BtnRefresh.Render();

    RenderFlatButton(m_BtnClaimSelected, L"Claim", hasSelection, 1);
    m_BtnClaimSelected.Render();

    RenderFlatButton(m_BtnClaimAll, L"Claim all", m_RowCount > 0, 2);
    m_BtnClaimAll.Render();

    RenderFlatButton(m_BtnClose, L"X", true, 2);
    m_BtnClose.Render();

    DisableAlphaBlend();

    return true;
}

const wchar_t* CNewUIMailbox::GetCurrencyText(BYTE currency, BYTE jewelSlot) const
{
    switch (currency)
    {
    case 0:
        return L"Zen";
    case 1:
        return L"W Coin";
    case 2:
        if (jewelSlot < 17)
            return s_MailboxJewelNames[jewelSlot];
        return L"Jewel";
    default:
        return L"?";
    }
}

const wchar_t* CNewUIMailbox::GetStatusText(BYTE status) const
{
    switch (status)
    {
    case 0:
        return L"Pending";
    case 1:
        return L"Bought";
    case 2:
        return L"Returned";
    case 3:
        return L"Expired";
    case 4:
        return L"Claimed";
    default:
        return L"?";
    }
}

const wchar_t* CNewUIMailbox::GetEntryTypeText(const EntryView& entry) const
{
    if (IsLikelyPayout(entry))
        return L"Payout";

    switch (entry.Status)
    {
    case 1:
        return L"Bought";
    case 2:
    case 3:
        return L"Return";
    default:
        return L"Item";
    }
}
