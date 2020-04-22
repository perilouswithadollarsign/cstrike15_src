#include <vgui_controls/JoypadFocus.h>
#include <vgui_controls/ImagePanelColored.h>
#include <vgui/MouseCode.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include "keydefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CJoypadFocus::CJoypadFocus()
{
	m_bJoypadMode = false;
	m_bDebugOutput = false;
	m_iModal = 0;

	m_FocusAreas.Purge();
	m_CurrentFocus.bClickOnFocus = false;
	m_CurrentFocus.hPanel = NULL;

	m_KeyNum[JF_KEY_UP] = K_AXISY_POS;
	m_KeyNum[JF_KEY_DOWN] = K_AXISY_NEG;
	m_KeyNum[JF_KEY_LEFT] = K_AXISX_NEG;
	m_KeyNum[JF_KEY_RIGHT] = K_AXISX_POS;
	m_KeyNum[JF_KEY_CONFIRM] = K_JOY1;
	m_KeyNum[JF_KEY_CANCEL] = K_JOY2;

	for (int i=0;i<NUM_JF_KEYS;i++)
	{
		m_bKeyDown[i] = false;
		m_fNextKeyRepeatTime[i] = 0;
	}
}

void CJoypadFocus::SetJoypadCodes(int iUpCode, int iDownCode, int iLeftCode, int iRightCode, int iConfirmCode, int iCancelCode)
{	
	m_KeyNum[JF_KEY_UP] = iUpCode;
	m_KeyNum[JF_KEY_DOWN] = iDownCode;
	m_KeyNum[JF_KEY_LEFT] = iLeftCode;
	m_KeyNum[JF_KEY_RIGHT] = iRightCode;
	m_KeyNum[JF_KEY_CONFIRM] = iConfirmCode;
	m_KeyNum[JF_KEY_CANCEL] = iCancelCode;	
}

void CJoypadFocus::AddToFocusList(vgui::Panel* pPanel, bool bClickOnFocus, bool bModal)
{
	if (!pPanel)
		return;

	FocusArea Focus;
	Focus.hPanel = pPanel;
	Focus.bClickOnFocus = bClickOnFocus;
	Focus.bModal = bModal;
	if (bModal)
		m_iModal++;
	vgui::PHandle hOtherPanel;
	for (int i=0;i<m_FocusAreas.Count();i++)
	{
		hOtherPanel = m_FocusAreas[i].hPanel;
		if (hOtherPanel == Focus.hPanel)	// check if it's already here
			return;
	}
	if (m_bDebugOutput)
		Msg("adding panel to joypad focus list: %s:%s\n", pPanel->GetName(), pPanel->GetClassName());
	//if (GetFocusPanel() == NULL)
	//{
		//SetFocusPanel(pPanel, bClickOnFocus);
	//}
	m_FocusAreas.AddToTail(Focus);
}

// sets focus to one of the panels in the list of focus areas
void CJoypadFocus::SetFocusPanel(int index)
{
	if (index < 0 || index >= m_FocusAreas.Count())
		return;

	SetFocusPanel(m_FocusAreas[index].hPanel, m_FocusAreas[index].bClickOnFocus);
}

void CJoypadFocus::SetFocusPanel(vgui::Panel* pPanel, bool bClickOnFocus)
{
	m_CurrentFocus.hPanel = pPanel;
	m_CurrentFocus.bClickOnFocus = bClickOnFocus;
	if (pPanel)
	{
		if (m_bDebugOutput)
			Msg("Setcur jfocus: %s:%s ", pPanel->GetName(), pPanel->GetClassName());
		if (!m_hOutline.Get())
		{
			CJoypadOutline *pOutline = new CJoypadOutline(NULL, "JoypadOutline");
			m_hOutline = pOutline;
			if (m_bDebugOutput)
			{
				if (pOutline)
					Msg("spawned outline\n");
				else
					Msg("Outline is zero!!\n");
			}
		}
		if (m_hOutline.Get())
		{
			if (pPanel->GetParent())
				m_hOutline->SetParent(pPanel->GetParent());
			else
				m_hOutline->SetParent(pPanel);
			int x, y, w, t;
			pPanel->GetJoypadCursorBounds(x, y, w, t);
			
			m_hOutline->MoveToFront();
			m_hOutline->SizeTo(x, y, w, t);
			m_hOutline->SetMouseInputEnabled(false);
		}
		if (bClickOnFocus)
		{
			ClickFocusPanel(true, false);
			ClickFocusPanel(false, false);
		}
	}
	else
	{
		if (m_bDebugOutput)
			Msg("Cleared currently focused joypad panel\n");
	}
}

vgui::Panel* CJoypadFocus::GetFocusPanel()
{
	return m_CurrentFocus.hPanel;
}

void CJoypadFocus::RemoveFromFocusList(vgui::Panel* pPanel)
{
	if (!pPanel)
		return;
	if (m_bDebugOutput)
		Msg("removing panel from joypad focus list: %s:%s\n", pPanel->GetName(), pPanel->GetClassName());
	vgui::PHandle hPanel;
	hPanel = pPanel;
	for (int i=m_FocusAreas.Count()-1;i>=0;i--)
	{
		if (m_FocusAreas[i].hPanel == hPanel)
		{
			if (m_FocusAreas[i].bModal)
				m_iModal--;
			m_FocusAreas.Remove(i);
		}
	}	
}

bool CJoypadFocus::OnJoypadButtonPressed(int keynum)
{
	if (!IsJoypadMode())
		return false;

	// don't allow multiple direction presses at once
	int iDirectionPress = -1;
	
	for (int i=0;i<4;i++)
	{
		if (m_KeyNum[i] == keynum)
			iDirectionPress = i;		
	}
	bool bDirectionAlreadyDown = false;
	if (iDirectionPress != -1)
	{		
		for (int i=0;i<4;i++)
		{
			if (m_bKeyDown[i] && i != iDirectionPress)	// if it's a direction we're already pushing, we allow it?
				bDirectionAlreadyDown = true;		
		}		
		// abort if we're pushing a direction and another direction is already down
		if (bDirectionAlreadyDown)
		{
			return true;
		}
	}	

	for (int i=0;i<NUM_JF_KEYS;i++)
	{
		if (m_KeyNum[i] == keynum)
		{
			m_bKeyDown[i] = true;
			m_fNextKeyRepeatTime[i] = vgui::system()->GetTimeMillis() + JF_KEY_REPEAT_DELAY;
		}
	}

	if (keynum == m_KeyNum[JF_KEY_UP])
	{
		int index = FindNextPanel(GetFocusPanel(), 270);
		if (index != -1)
			SetFocusPanel(index);
		return (GetFocusPanel() != NULL);
	}
	else if (keynum == m_KeyNum[JF_KEY_DOWN])
	{
		int index = FindNextPanel(GetFocusPanel(), 90);
		if (index != -1)
			SetFocusPanel(index);
		return (GetFocusPanel() != NULL);
	}
	else if (keynum == m_KeyNum[JF_KEY_LEFT])
	{
		int index = FindNextPanel(GetFocusPanel(), 180);
		if (index != -1)
			SetFocusPanel(index);
		return (GetFocusPanel() != NULL);
	}
	else if (keynum == m_KeyNum[JF_KEY_RIGHT])
	{
		int index = FindNextPanel(GetFocusPanel(), 0);
		if (index != -1)
			SetFocusPanel(index);
		return (GetFocusPanel() != NULL);
	}
	else if (keynum == m_KeyNum[JF_KEY_CONFIRM])
	{
		if (m_CurrentFocus.bClickOnFocus)
			DoubleClickFocusPanel(false);
		else
			ClickFocusPanel(true, false);

		return (GetFocusPanel() != NULL);
	}
	else if (keynum == m_KeyNum[JF_KEY_CANCEL])
	{		
		ClickFocusPanel(true, true);
		return (GetFocusPanel() != NULL);
	}
	// don't swallow non-direction or confirm/cancel keys
	return false;
}

bool CJoypadFocus::OnJoypadButtonReleased(int keynum)
{
	if (!IsJoypadMode())
		return false;

	for (int i=0;i<NUM_JF_KEYS;i++)
	{
		if (m_KeyNum[i] == keynum)
		{
			m_bKeyDown[i] = false;
			m_fNextKeyRepeatTime[i] = 0;
		}
	}

	if (keynum == m_KeyNum[JF_KEY_CONFIRM])
	{
		ClickFocusPanel(false, false);
		return (GetFocusPanel() != NULL);
	}
	else if (keynum == m_KeyNum[JF_KEY_CANCEL])
	{
		ClickFocusPanel(false, true);
		return (GetFocusPanel() != NULL);
	}
	// don't swallow non confirm/cancel buttons
	return false;
}

void CJoypadFocus::CheckKeyRepeats()
{
	float curtime = vgui::system()->GetTimeMillis();
	for (int i=0;i<NUM_JF_KEYS;i++)
	{
		if (m_fNextKeyRepeatTime[i]!=0 && curtime > m_fNextKeyRepeatTime[i])
		{
			// player is holding down the specified key, send another press
			if (m_bDebugOutput)
				Msg("Sending key repeat\n");
			OnJoypadButtonPressed(m_KeyNum[i]);
			m_fNextKeyRepeatTime[i] = curtime + JF_KEY_REPEAT_INTERVAL;
		}
	}	
}

void CJoypadFocus::ClickFocusPanel(bool bDown, bool bRightMouse)
{	
	vgui::Panel *pOther = GetFocusPanel();
	if (pOther)
	{
		if (bDown)
			pOther->OnMousePressed(bRightMouse ? vgui::MOUSE_RIGHT : vgui::MOUSE_LEFT);
		else
			pOther->OnMouseReleased(bRightMouse ? vgui::MOUSE_RIGHT : vgui::MOUSE_LEFT);
	}
}

void CJoypadFocus::DoubleClickFocusPanel(bool bRightMouse)
{
	vgui::Panel *pOther = GetFocusPanel();
	if (pOther)
	{
		pOther->OnMouseDoublePressed(bRightMouse ? vgui::MOUSE_RIGHT : vgui::MOUSE_LEFT);
	}
}

// find a panel in the specified direction
int CJoypadFocus::FindNextPanel(vgui::Panel *pSource, float angle)
{
	if (!pSource)
	{
		// no panel selected, should pick the top left most one
		int iBestIndex = -1;
		float fBestRating = -1;
		for (int i=0;i<m_FocusAreas.Count();i++)
		{
			if (m_iModal>0 && !m_FocusAreas[i].bModal)
				continue;
			vgui::Panel *pPanel = m_FocusAreas[i].hPanel;
			if (!pPanel)
				continue;

			int x, y;
			pPanel->GetPos(x, y);
			float fRating = x * 1.3 + y;
			if (fBestRating == -1 || fRating < fBestRating)
			{
				fBestRating = fRating;
				iBestIndex = i;
			}
		}
		return iBestIndex;
	}

	if (m_bDebugOutput)
	Msg("angle = %f ", angle);
	float radangle = angle * (3.14159265f / 180.0f);
	float xdir = cos(radangle);
	float ydir = sin(radangle);
	if (m_bDebugOutput)
		Msg("xdir = %f ydir = %f\n", xdir, ydir);
	
	//Vector2D dir(xdir, ydir);
	//dir.NormalizeInPlace();	// normalization unnecessary?

	// find the centre of our panel
	int x, y, w, t;
	pSource->GetBounds(x, y, w, t);
	int posx = w * 0.5f;
	int posy = t * 0.5f;	
	pSource->LocalToScreen(posx, posy);	

	// position the source dot at the middle of the edge in the direction we're searching in
	posx += (w * 0.5f) * xdir - xdir;
	posy += (t * 0.5f) * ydir - ydir;

	//Vector2D vecSource(posx, posy);

	// go through all panels, see if they're in the right direction and rate them
	vgui::Panel *pBest = NULL;
	int iBestIndex = -1;
	float fBestRating = -1;
	for (int i=0;i<m_FocusAreas.Count();i++)
	{
		vgui::Panel* pOther = m_FocusAreas[i].hPanel;
		if (!pOther || pOther == pSource || !IsPanelReallyVisible(pOther))
			continue;
		if (m_iModal>0 && !m_FocusAreas[i].bModal)
			continue;
		// check if it's within our arc
		int w2, t2;
		pOther->GetSize(w2, t2);
		int posx2 = w2 * 0.5f;
		int posy2 = t2 * 0.5f;
		pOther->LocalToScreen(posx2, posy2);		
		// pick the point in our bounds closest to the source point
		if (posx < posx2)
			posx2 = max((posx2 - w2 * 0.5f), posx);
		else if (posx > posx2)
			posx2 = min((posx2 + w2 * 0.5f), posx);

		if (posy < posy2)
			posy2 = max((posy2 - t2 * 0.5f), posy);
		else if (posy > posy2)
			posy2 = min((posy2 + t2 * 0.5f), posy);
		//Vector2D vecOther(posx2, posy2);

		float diffx = posx2 - posx;
		float diffy = posy2 - posy;
		float diff_len = sqrt(diffx * diffx + diffy * diffy);
		if (diff_len <= 0)
			diff_len = 0.1f;
		float diffx_norm = diffx / diff_len;
		float diffy_norm = diffy / diff_len;

		float the_dot = 0;
		the_dot += diffx_norm * xdir;
		the_dot += diffy_norm * ydir;			
		//Vector2D difference;
		//difference = vecOther - vecSource;
		//Vector2D diffnorm = difference;
		//diffnorm.NormalizeInPlace();
		if (m_bDebugOutput)
			Msg("Checking panel %i (%s). diff=%f,%f dir=%f, %f dot=%f\n", i, pOther->GetClassName(), diffx, diffy, xdir, ydir, the_dot);

		if (the_dot > 0.3f)
		{
			// this panel is in the right direction, now rate it
			float fRating = -1;
			if (angle == 0 || angle == 90 || angle == 180 || angle == 270)	// we're searching in a perpendicular direction, so we can do a more accurate rating
			{
				fRating = 0;
				// double perpendicular distance cost
				if (angle == 90 || angle == 270)
				{
					if (m_bDebugOutput)
						Msg("  vertical rating: %f * 3 + %f\n", fabs(diffx), fabs(diffy));
					fRating = fabs(diffy) + (fabs(diffx) * 3.0f);
				}
				else
				{
					if (m_bDebugOutput)
						Msg("  horiz rating: %f + %f * 3\n", fabs(diffx), fabs(diffy));
					fRating = fabs(diffx) + (fabs(diffy) * 3.0f);
				}
			}
			else	// strange angle, just rate based on distance
			{
				if (m_bDebugOutput)
					Msg("  distancebased rating\n");
				fRating = diff_len;
			}
			if (m_bDebugOutput)
				Msg("  Panel is in right dir, rating = %f\n", fRating);
			// if this panel is better, remember it
			if (pBest == NULL || (fRating != -1 && fRating < fBestRating))
			{
				if (m_bDebugOutput)
					Msg("  this is the new best!\n");
				pBest = pOther;
				iBestIndex = i;
				fBestRating = fRating;
			}
		}
	}
	return iBestIndex;
}

bool CJoypadFocus::IsPanelReallyVisible(vgui::Panel *pPanel)
{
	while (pPanel->IsVisible() && pPanel->GetAlpha() >= 255)	// todo: make required alpha an arg
	{	
		pPanel = pPanel->GetParent();
		if (!pPanel)
			return true;		// got to the top without hitting something that wasn't visible
	}
	return false;
}

// ================================================================================

CJoypadFocus* g_pJoypadFocus = NULL;

CJoypadFocus* GetJoypadFocus()
{
	if (g_pJoypadFocus == NULL)
	{
		g_pJoypadFocus = new CJoypadFocus();
	}
	return g_pJoypadFocus;
}

// ================================================================================

CJoypadOutline::CJoypadOutline(vgui::Panel *parent, const char *name) : vgui::Panel(parent, name)
{
	SetMouseInputEnabled(false);
	SetAlpha(0);
	m_hLastFocusPanel = NULL;
	//m_pImagePanel = new vgui::ImagePanelColored(this, "ImagePanel");
	//m_pImagePanel->SetImage("swarm/JoypadCursor");
	//m_pImagePanel->SetShouldScaleImage(true);	
}

void CJoypadOutline::ApplySchemeSettings(vgui::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled(false);
	//SetPaintBackgroundType(0);
	//SetBgColor(Color(255,0,0,64));
}

void CJoypadOutline::Paint()
{			
	long curtime = vgui::system()->GetTimeMillis();
	int pulse_time = 1000;
	int half_pulse_time = pulse_time / 2;
	long remainder = curtime % pulse_time;
	if (remainder > half_pulse_time)
		remainder = half_pulse_time - (remainder - half_pulse_time);
	float fWhite = float(remainder) / float(half_pulse_time);
	//int red = 66.0f + (255.0f - 66.0f) * fWhite;
	//int green = 142.0f  + (255.0f - 142.0f) * fWhite;
	//int blue = 192.0f + (255.0f - 192.0f) * fWhite;
	int red = 255.0f + (255.0f - 255) * fWhite;
	int green = 0  + (255.0f - 0) * fWhite;
	int blue = 0;
	Color col(red, green, blue, 255);	
	DrawBox( 0, 0, GetWide(), GetTall(), col, 0.8f, true );
}

void CJoypadOutline::GetCornerTextureSize( int& w, int& h )
{
	BaseClass::GetCornerTextureSize(w, h);

	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	float fScale = sh / 768.0f;
	w *= fScale * 0.25f;
	h *= fScale * 0.25f;
}

// hide/show us and position us over the focused panel
void CJoypadOutline::OnThink()
{
	if (!GetJoypadFocus())
		return;
	
	vgui::Panel* pFocus = GetJoypadFocus()->GetFocusPanel();
	//Msg("outline thinking, focus = %s\n", pFocus ? pFocus->GetClassName() : "none");
	if (pFocus && GetJoypadFocus()->IsJoypadMode())
	{
		if (pFocus != m_hLastFocusPanel.Get())
		{
			m_hLastFocusPanel = pFocus;
			SetAlpha(255);
		}
		if (pFocus->GetParent())
			SetParent(pFocus->GetParent());
		else
			SetParent(pFocus);
		// make sure we're positioned over the focused panel
		int x, y, w, t;
		pFocus->GetJoypadCursorBounds(x, y, w, t);
		SizeTo(x, y, w, t);

		GetJoypadFocus()->CheckKeyRepeats();

		SetMouseInputEnabled(false);
	}
	else
	{
		SetAlpha(0);
		SetMouseInputEnabled(false);
	}
}

void CJoypadOutline::SizeTo(int x, int y, int w, int t)
{
	SetBounds(x, y, w, t);
	//m_pImagePanel->SetBounds(0, 0, w, t);
}