#include <windows.h>

/* test_bitboxes() - use a static text box for selecting/changing a list of bits, hex bytes,
	or other evenly spaced things. */
/* scrPos.x = x coord */
/* scrPos.y = y coord */
/* box_id = an array of dialog ID's, one for each box */
/* ndiv = number of divisions per box */
/* nboxes = number of boxes */
/* return value = selection number or -1 if point is outside of all boxes */
int
test_bitboxes( HWND hDlg, unsigned long pos, unsigned ndiv, int nboxes, const int *box_id )
{
	int i;
	POINT scrPos;

	/* find current position in screen coordinates. */
	scrPos.x = (int)LOWORD(pos);
	scrPos.y = (int)HIWORD(pos);
	ClientToScreen(hDlg, &scrPos);

	for( i = 0; i < nboxes; i++ ) { 
		RECT box;
		HWND hWndItem;
		HDC hDC;
		static char buf[100];

		/* Check point against bounding box of text box */

		hWndItem = GetDlgItem(hDlg, box_id[i]);
		GetWindowRect(hWndItem, &box);
		GetWindowText(hWndItem, buf, sizeof(buf));
		hDC = GetDC(hWndItem);
		if (hDC) {
			SIZE stringSize;
			GetTextExtentPoint(hDC, buf, strlen(buf), &stringSize);

			// heuristic rule: sometimes GetTextExtentPoint lies
			// when the font is manually reduced in size, say 70%.
			// So, only believe the string size if it is smaller
			// than the box size.
			if (stringSize.cx < (box.right - box.left)) {
				// now correct the right side of box for string size.
				box.right=box.left + stringSize.cx;
			}
			ReleaseDC(hWndItem,hDC);
		}
		

		if( scrPos.x > box.left && scrPos.x < box.right && 
			scrPos.y > box.top  && scrPos.y < box.bottom ) {

			/* Check which character the point is in */
		return (ndiv*i + (ndiv*(scrPos.x - box.left)) / (box.right - box.left));
		}
	}
	return -1;
}
