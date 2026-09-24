option casemap:none
EXTERN g_DirectInput8Create:QWORD
EXTERN g_DllCanUnloadNow:QWORD
EXTERN g_DllGetClassObject:QWORD
EXTERN g_DllRegisterServer:QWORD
EXTERN g_DllUnregisterServer:QWORD
EXTERN g_GetdfDIJoystick:QWORD
EXTERN g_WH3DLSS_AcquirePostDepthV2:QWORD
EXTERN g_WH3DLSS_EndPostDepthFrameV2:QWORD
.code
DirectInput8Create PROC
    jmp QWORD PTR [g_DirectInput8Create]
DirectInput8Create ENDP
DllCanUnloadNow PROC
    jmp QWORD PTR [g_DllCanUnloadNow]
DllCanUnloadNow ENDP
DllGetClassObject PROC
    jmp QWORD PTR [g_DllGetClassObject]
DllGetClassObject ENDP
DllRegisterServer PROC
    jmp QWORD PTR [g_DllRegisterServer]
DllRegisterServer ENDP
DllUnregisterServer PROC
    jmp QWORD PTR [g_DllUnregisterServer]
DllUnregisterServer ENDP
GetdfDIJoystick PROC
    jmp QWORD PTR [g_GetdfDIJoystick]
GetdfDIJoystick ENDP
WH3DLSS_AcquirePostDepthV2 PROC
    jmp QWORD PTR [g_WH3DLSS_AcquirePostDepthV2]
WH3DLSS_AcquirePostDepthV2 ENDP
WH3DLSS_EndPostDepthFrameV2 PROC
    jmp QWORD PTR [g_WH3DLSS_EndPostDepthFrameV2]
WH3DLSS_EndPostDepthFrameV2 ENDP
END
