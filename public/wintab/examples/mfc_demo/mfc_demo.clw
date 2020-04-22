; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CMFC_DEMOView
LastTemplate=CDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "MFC_DEMO.h"
LastPage=0

ClassCount=8
Class1=CMFC_DEMOApp
Class2=CMFC_DEMODoc
Class3=CMFC_DEMOView
Class4=CMainFrame
Class7=CAboutDlg

ResourceCount=5
Resource1=IDD_ABOUTBOX
Resource2=IDR_MAINFRAME

[CLS:CMFC_DEMOApp]
Type=0
HeaderFile=MFC_DEMO.h
ImplementationFile=MFC_DEMO.cpp
Filter=N

[CLS:CMFC_DEMODoc]
Type=0
HeaderFile=MFC_DEMODoc.h
ImplementationFile=MFC_DEMODoc.cpp
Filter=N

[CLS:CMFC_DEMOView]
Type=0
HeaderFile=MFC_DEMOView.h
ImplementationFile=MFC_DEMOView.cpp
Filter=C
BaseClass=CView
VirtualFilter=VWC

[CLS:CMainFrame]
Type=0
HeaderFile=MainFrm.h
ImplementationFile=MainFrm.cpp
Filter=T
BaseClass=CFrameWnd
VirtualFilter=fWC



[CLS:CAboutDlg]
Type=0
HeaderFile=MFC_DEMO.cpp
ImplementationFile=MFC_DEMO.cpp
Filter=D

[DLG:IDD_ABOUTBOX]
Type=1
Class=CAboutDlg
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889

[MNU:IDR_MAINFRAME]
Type=1
Class=CMainFrame
Command1=ID_FILE_NEW
Command2=ID_APP_EXIT
Command3=ID_APP_ABOUT
CommandCount=3

[ACL:IDR_MAINFRAME]
Type=1
Class=CMainFrame
Command1=ID_FILE_NEW
Command2=ID_FILE_OPEN
Command3=ID_FILE_SAVE
Command4=ID_EDIT_UNDO
Command5=ID_EDIT_CUT
Command6=ID_EDIT_COPY
Command7=ID_EDIT_PASTE
Command8=ID_EDIT_UNDO
Command9=ID_EDIT_CUT
Command10=ID_EDIT_COPY
Command11=ID_EDIT_PASTE
Command12=ID_NEXT_PANE
Command13=ID_PREV_PANE
CommandCount=13

