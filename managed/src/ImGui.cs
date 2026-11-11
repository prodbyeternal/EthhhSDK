// ===========================================================================
//  EthhhSDK.Managed - Dear ImGui wrapper for C# mods
//
//  Thin, allocation-free (except string marshaling) facade over the
//  EthImGui_* C ABI. Every function available to Lua is available here.
// ===========================================================================
using System;
using System.Runtime.InteropServices;

namespace EthhhSDK
{
    internal static class NativeImGui
    {
        private const string Dll = Native.Dll;

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_EnsureContext();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr EthImGui_GetVersion();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern float EthImGui_GetScale();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint EthImGui_GetAccent();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint EthImGui_RGBA(float r, float g, float b, float a);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint EthImGui_GetStyleColor(int index);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PushAccentText();

        // windows
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_Begin([MarshalAs(UnmanagedType.LPUTF8Str)] string name, int flags);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginClosable([MarshalAs(UnmanagedType.LPUTF8Str)] string name, ref int open, int flags);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_End();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginChild([MarshalAs(UnmanagedType.LPUTF8Str)] string id, float w, float h, int border);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_EndChild();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginTabBar([MarshalAs(UnmanagedType.LPUTF8Str)] string id);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_EndTabBar();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginTabItem([MarshalAs(UnmanagedType.LPUTF8Str)] string label, int selected);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_EndTabItem();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginTable([MarshalAs(UnmanagedType.LPUTF8Str)] string id, int columns, int flags);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_EndTable();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_TableSetupColumn([MarshalAs(UnmanagedType.LPUTF8Str)] string label, int flags, float width);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_TableHeadersRow();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_TableNextRow();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_TableNextColumn();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_TableSetColumnEnabled(int column, int enabled);

        // text
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_Text([MarshalAs(UnmanagedType.LPUTF8Str)] string text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_TextColored(uint col, [MarshalAs(UnmanagedType.LPUTF8Str)] string text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_TextDisabled([MarshalAs(UnmanagedType.LPUTF8Str)] string text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_LabelText([MarshalAs(UnmanagedType.LPUTF8Str)] string label, [MarshalAs(UnmanagedType.LPUTF8Str)] string text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_BulletText([MarshalAs(UnmanagedType.LPUTF8Str)] string text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_Separator();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SeparatorText([MarshalAs(UnmanagedType.LPUTF8Str)] string text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SameLine(float offset, float spacing);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_Spacing();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_Dummy(float w, float h);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_NewLine();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_AlignTextToFramePadding();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_Indent(float amount);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_Unindent(float amount);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PushTextWrapPos(float pos);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PopTextWrapPos();

        // widgets
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_Button([MarshalAs(UnmanagedType.LPUTF8Str)] string label, float w, float h);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_SmallButton([MarshalAs(UnmanagedType.LPUTF8Str)] string label);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_InvisibleButton([MarshalAs(UnmanagedType.LPUTF8Str)] string id, float w, float h);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_Checkbox([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref int value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_RadioButton([MarshalAs(UnmanagedType.LPUTF8Str)] string label, int active);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_Selectable([MarshalAs(UnmanagedType.LPUTF8Str)] string label, int selected);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_SliderFloat([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref float value, float minV, float maxV, [MarshalAs(UnmanagedType.LPUTF8Str)] string fmt);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_SliderInt([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref int value, int minV, int maxV);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_SliderFloat4([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref float values, float minV, float maxV);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_DragFloat([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref float value, float speed, float minV, float maxV, [MarshalAs(UnmanagedType.LPUTF8Str)] string fmt);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_DragInt([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref int value, float speed, int minV, int maxV);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_InputText([MarshalAs(UnmanagedType.LPUTF8Str)] string label, IntPtr buffer, int bufferSize);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_InputInt([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref int value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_InputFloat([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref float value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginCombo([MarshalAs(UnmanagedType.LPUTF8Str)] string label, [MarshalAs(UnmanagedType.LPUTF8Str)] string preview);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_EndCombo();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_Combo([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref int current,
            [MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.LPStr)] string[] items, int count);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_ProgressBar(float fraction, float w, float h, [MarshalAs(UnmanagedType.LPUTF8Str)] string overlay);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PlotLines([MarshalAs(UnmanagedType.LPUTF8Str)] string label, float[] values, int count, float height);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_ColorEdit3([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref float rgb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_ColorEdit4([MarshalAs(UnmanagedType.LPUTF8Str)] string label, ref float rgba);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_CollapsingHeader([MarshalAs(UnmanagedType.LPUTF8Str)] string label, int defaultOpen);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_TreeNode([MarshalAs(UnmanagedType.LPUTF8Str)] string label, int flags);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_TreePop();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetNextItemOpen(int open);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PushIDStr([MarshalAs(UnmanagedType.LPUTF8Str)] string id);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PushIDInt(int id);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PopID();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PushItemWidth(float width);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PopItemWidth();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetNextItemWidth(float width);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PushStyleColor(int index, uint color);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PopStyleColor(int count);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PushStyleVarFloat(int index, float value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PushStyleVarVec2(int index, float x, float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PopStyleVar(int count);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PushFont([MarshalAs(UnmanagedType.LPUTF8Str)] string which);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_PopFont();

        // positioning / queries
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetNextWindowPos(float x, float y, int cond);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetNextWindowSize(float w, float h, int cond);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetNextWindowBgAlpha(float alpha);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginTooltip();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_EndTooltip();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetTooltip([MarshalAs(UnmanagedType.LPUTF8Str)] string text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginPopup([MarshalAs(UnmanagedType.LPUTF8Str)] string id);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_EndPopup();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_OpenPopup([MarshalAs(UnmanagedType.LPUTF8Str)] string id);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_CloseCurrentPopup();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginPopupContextItem([MarshalAs(UnmanagedType.LPUTF8Str)] string id);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginMenuBar();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_EndMenuBar();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_BeginMenu([MarshalAs(UnmanagedType.LPUTF8Str)] string label);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_EndMenu();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_MenuItem([MarshalAs(UnmanagedType.LPUTF8Str)] string label, [MarshalAs(UnmanagedType.LPUTF8Str)] string shortcut, int selected, int enabled);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_GetCursorScreenPos(out float x, out float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetCursorScreenPos(float x, float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_GetCursorPos(out float x, out float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetCursorPos(float x, float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_GetContentRegionAvail(out float x, out float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_GetWindowPos(out float x, out float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_GetWindowSize(out float x, out float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_CalcTextSize([MarshalAs(UnmanagedType.LPUTF8Str)] string text, out float w, out float h);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern float EthImGui_GetTextLineHeight();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern float EthImGui_GetFrameHeight();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_GetItemRectMin(out float x, out float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_GetItemRectMax(out float x, out float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_GetItemRectSize(out float x, out float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsItemHovered();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsItemClicked(int button);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsItemActive();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsItemEdited();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsItemVisible();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsWindowHovered();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsWindowFocused();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsAnyItemActive();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetItemDefaultFocus();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetKeyboardFocusHere();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsKeyDown(int key);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_GetMousePos(out float x, out float y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsMouseDown(int button);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_IsMouseClicked(int button);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_GetIO(out float deltaTime, out float framerate, out float w, out float h);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_WantCaptureMouse();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthImGui_WantCaptureKeyboard();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_SetMouseCursor(int cursor);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern float EthImGui_GetTime();

        // drawing
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_DrawLine(float x1, float y1, float x2, float y2, uint col, float thickness);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_DrawRect(float x1, float y1, float x2, float y2, uint col, float rounding, float thickness);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_DrawRectFilled(float x1, float y1, float x2, float y2, uint col, float rounding);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_DrawCircle(float cx, float cy, float radius, uint col, int segments, float thickness);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_DrawCircleFilled(float cx, float cy, float radius, uint col, int segments);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_DrawText(float x, float y, uint col, [MarshalAs(UnmanagedType.LPUTF8Str)] string text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_DrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, uint col, float thickness);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_DrawTriangleFilled(float x1, float y1, float x2, float y2, float x3, float y3, uint col);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_DrawQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint col, float thickness);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthImGui_DrawQuadFilled(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint col);
    }

    /// <summary>Dear ImGui facade for C# mods.</summary>
    public static class ImGui
    {
        public static void EnsureContext() => NativeImGui.EthImGui_EnsureContext();
        public static string Version => Marshal.PtrToStringAnsi(NativeImGui.EthImGui_GetVersion());
        public static float Scale => NativeImGui.EthImGui_GetScale();
        public static uint Accent => NativeImGui.EthImGui_GetAccent();
        public static float Time => NativeImGui.EthImGui_GetTime();
        public static uint RGBA(float r, float g, float b, float a) => NativeImGui.EthImGui_RGBA(r, g, b, a);
        public static uint StyleColor(ImGuiCol index) => NativeImGui.EthImGui_GetStyleColor((int)index);
        public static void PushAccentText() => NativeImGui.EthImGui_PushAccentText();

        // ------------------------------------------------------------- windows
        public static bool Begin(string name, ImGuiWindowFlags flags = 0) => NativeImGui.EthImGui_Begin(name, (int)flags) != 0;
        public static bool BeginClosable(string name, ref bool open, ImGuiWindowFlags flags = 0)
        {
            int v = open ? 1 : 0;
            bool result = NativeImGui.EthImGui_BeginClosable(name, ref v, (int)flags) != 0;
            open = v != 0;
            return result;
        }
        public static void End() => NativeImGui.EthImGui_End();
        public static bool BeginChild(string id, float w = 0, float h = 0, bool border = false) => NativeImGui.EthImGui_BeginChild(id, w, h, border ? 1 : 0) != 0;
        public static void EndChild() => NativeImGui.EthImGui_EndChild();
        public static bool BeginTabBar(string id) => NativeImGui.EthImGui_BeginTabBar(id) != 0;
        public static void EndTabBar() => NativeImGui.EthImGui_EndTabBar();
        public static bool BeginTabItem(string label, bool selected = false) => NativeImGui.EthImGui_BeginTabItem(label, selected ? 1 : 0) != 0;
        public static void EndTabItem() => NativeImGui.EthImGui_EndTabItem();
        public static bool BeginTable(string id, int columns, ImGuiTableFlags flags = 0) => NativeImGui.EthImGui_BeginTable(id, columns, (int)flags) != 0;
        public static void EndTable() => NativeImGui.EthImGui_EndTable();
        public static void TableSetupColumn(string label, ImGuiTableColumnFlags flags = 0, float width = 0) => NativeImGui.EthImGui_TableSetupColumn(label, (int)flags, width);
        public static void TableHeadersRow() => NativeImGui.EthImGui_TableHeadersRow();
        public static void TableNextRow() => NativeImGui.EthImGui_TableNextRow();
        public static bool TableNextColumn() => NativeImGui.EthImGui_TableNextColumn() != 0;
        public static void TableSetColumnEnabled(int column, bool enabled) => NativeImGui.EthImGui_TableSetColumnEnabled(column, enabled ? 1 : 0);

        // ---------------------------------------------------------------- text
        public static void Text(string text) => NativeImGui.EthImGui_Text(text ?? "");
        public static void TextColored(uint col, string text) => NativeImGui.EthImGui_TextColored(col, text ?? "");
        public static void TextColored(ImVec4 col, string text) => NativeImGui.EthImGui_TextColored(col.ToU32(), text ?? "");
        public static void TextDisabled(string text) => NativeImGui.EthImGui_TextDisabled(text ?? "");
        public static void LabelText(string label, string text) => NativeImGui.EthImGui_LabelText(label, text ?? "");
        public static void BulletText(string text) => NativeImGui.EthImGui_BulletText(text ?? "");
        public static void Separator() => NativeImGui.EthImGui_Separator();
        public static void SeparatorText(string text) => NativeImGui.EthImGui_SeparatorText(text ?? "");
        public static void SameLine(float offset = 0, float spacing = -1) => NativeImGui.EthImGui_SameLine(offset, spacing);
        public static void Spacing() => NativeImGui.EthImGui_Spacing();
        public static void Dummy(float w, float h) => NativeImGui.EthImGui_Dummy(w, h);
        public static void NewLine() => NativeImGui.EthImGui_NewLine();
        public static void AlignTextToFramePadding() => NativeImGui.EthImGui_AlignTextToFramePadding();
        public static void Indent(float amount = 21.0f) => NativeImGui.EthImGui_Indent(amount);
        public static void Unindent(float amount = 21.0f) => NativeImGui.EthImGui_Unindent(amount);
        public static void PushTextWrapPos(float pos = 0.0f) => NativeImGui.EthImGui_PushTextWrapPos(pos);
        public static void PopTextWrapPos() => NativeImGui.EthImGui_PopTextWrapPos();

        // ------------------------------------------------------------- widgets
        public static bool Button(string label, float w = 0, float h = 0) => NativeImGui.EthImGui_Button(label, w, h) != 0;
        public static bool SmallButton(string label) => NativeImGui.EthImGui_SmallButton(label) != 0;
        public static bool InvisibleButton(string id, float w, float h) => NativeImGui.EthImGui_InvisibleButton(id, w, h) != 0;
        public static bool Checkbox(string label, ref bool value)
        {
            int v = value ? 1 : 0;
            bool changed = NativeImGui.EthImGui_Checkbox(label, ref v) != 0;
            value = v != 0;
            return changed;
        }
        public static bool RadioButton(string label, bool active) => NativeImGui.EthImGui_RadioButton(label, active ? 1 : 0) != 0;
        public static bool Selectable(string label, bool selected = false) => NativeImGui.EthImGui_Selectable(label, selected ? 1 : 0) != 0;
        public static bool SliderFloat(string label, ref float value, float min, float max, string format = "%.3f")
            => NativeImGui.EthImGui_SliderFloat(label, ref value, min, max, format) != 0;
        public static bool SliderInt(string label, ref int value, int min, int max)
            => NativeImGui.EthImGui_SliderInt(label, ref value, min, max) != 0;
        public static bool SliderFloat4(string label, float[] values, float min, float max)
            => NativeImGui.EthImGui_SliderFloat4(label, ref values[0], min, max) != 0;
        public static bool DragFloat(string label, ref float value, float speed = 1.0f, float min = 0, float max = 0, string format = "%.3f")
            => NativeImGui.EthImGui_DragFloat(label, ref value, speed, min, max, format) != 0;
        public static bool DragInt(string label, ref int value, float speed = 1.0f, int min = 0, int max = 0)
            => NativeImGui.EthImGui_DragInt(label, ref value, speed, min, max) != 0;
        public static bool InputInt(string label, ref int value) => NativeImGui.EthImGui_InputInt(label, ref value) != 0;
        public static bool InputFloat(string label, ref float value) => NativeImGui.EthImGui_InputFloat(label, ref value) != 0;
        public static bool InputText(string label, ref string value, int maxLength = 256)
        {
            IntPtr mem = Marshal.AllocHGlobal(maxLength + 1);
            try
            {
                byte[] bytes = System.Text.Encoding.UTF8.GetBytes(value ?? "");
                int count = Math.Min(bytes.Length, maxLength);
                Marshal.Copy(bytes, 0, mem, count);
                Marshal.WriteByte(mem, count, 0);
                bool changed = NativeImGui.EthImGui_InputText(label, mem, maxLength + 1) != 0;
                if (changed)
                    value = Utf8ToString(mem) ?? "";
                return changed;
            }
            finally { Marshal.FreeHGlobal(mem); }
        }

        internal static string Utf8ToString(IntPtr ptr)
        {
            if (ptr == IntPtr.Zero) return null;
            int len = 0;
            while (Marshal.ReadByte(ptr, len) != 0) len++;
            if (len == 0) return string.Empty;
            byte[] bytes = new byte[len];
            Marshal.Copy(ptr, bytes, 0, len);
            return System.Text.Encoding.UTF8.GetString(bytes);
        }
        public static bool BeginCombo(string label, string preview) => NativeImGui.EthImGui_BeginCombo(label, preview ?? "") != 0;
        public static void EndCombo() => NativeImGui.EthImGui_EndCombo();
        public static bool Combo(string label, ref int current, string[] items)
            => NativeImGui.EthImGui_Combo(label, ref current, items, items.Length) != 0;
        public static void ProgressBar(float fraction, float w = -1, float h = 0, string overlay = null)
            => NativeImGui.EthImGui_ProgressBar(fraction, w, h, overlay);
        public static void PlotLines(string label, float[] values, float height = 0)
            => NativeImGui.EthImGui_PlotLines(label, values, values.Length, height);
        public static bool ColorEdit3(string label, float[] rgb) => NativeImGui.EthImGui_ColorEdit3(label, ref rgb[0]) != 0;
        public static bool ColorEdit4(string label, float[] rgba) => NativeImGui.EthImGui_ColorEdit4(label, ref rgba[0]) != 0;
        public static bool CollapsingHeader(string label, bool defaultOpen = true) => NativeImGui.EthImGui_CollapsingHeader(label, defaultOpen ? 1 : 0) != 0;
        public static bool TreeNode(string label, ImGuiTreeNodeFlags flags = 0) => NativeImGui.EthImGui_TreeNode(label, (int)flags) != 0;
        public static void TreePop() => NativeImGui.EthImGui_TreePop();
        public static void SetNextItemOpen(bool open) => NativeImGui.EthImGui_SetNextItemOpen(open ? 1 : 0);
        public static void PushID(string id) => NativeImGui.EthImGui_PushIDStr(id);
        public static void PushID(int id) => NativeImGui.EthImGui_PushIDInt(id);
        public static void PopID() => NativeImGui.EthImGui_PopID();
        public static void PushItemWidth(float width) => NativeImGui.EthImGui_PushItemWidth(width);
        public static void PopItemWidth() => NativeImGui.EthImGui_PopItemWidth();
        public static void SetNextItemWidth(float width) => NativeImGui.EthImGui_SetNextItemWidth(width);
        public static void PushStyleColor(ImGuiCol index, uint color) => NativeImGui.EthImGui_PushStyleColor((int)index, color);
        public static void PopStyleColor(int count = 1) => NativeImGui.EthImGui_PopStyleColor(count);
        public static void PushStyleVar(ImGuiStyleVar index, float value) => NativeImGui.EthImGui_PushStyleVarFloat((int)index, value);
        public static void PushStyleVar(ImGuiStyleVar index, float x, float y) => NativeImGui.EthImGui_PushStyleVarVec2((int)index, x, y);
        public static void PopStyleVar(int count = 1) => NativeImGui.EthImGui_PopStyleVar(count);
        public static void PushFont(string which) => NativeImGui.EthImGui_PushFont(which);
        public static void PopFont() => NativeImGui.EthImGui_PopFont();

        // ------------------------------------------------------------ positioning
        public static void SetNextWindowPos(float x, float y, ImGuiCond cond = 0) => NativeImGui.EthImGui_SetNextWindowPos(x, y, (int)cond);
        public static void SetNextWindowSize(float w, float h, ImGuiCond cond = 0) => NativeImGui.EthImGui_SetNextWindowSize(w, h, (int)cond);
        public static void SetNextWindowBgAlpha(float alpha) => NativeImGui.EthImGui_SetNextWindowBgAlpha(alpha);
        public static bool BeginTooltip() => NativeImGui.EthImGui_BeginTooltip() != 0;
        public static void EndTooltip() => NativeImGui.EthImGui_EndTooltip();
        public static void SetTooltip(string text) => NativeImGui.EthImGui_SetTooltip(text ?? "");
        public static bool BeginPopup(string id) => NativeImGui.EthImGui_BeginPopup(id) != 0;
        public static void EndPopup() => NativeImGui.EthImGui_EndPopup();
        public static void OpenPopup(string id) => NativeImGui.EthImGui_OpenPopup(id);
        public static void CloseCurrentPopup() => NativeImGui.EthImGui_CloseCurrentPopup();
        public static bool BeginPopupContextItem(string id = null) => NativeImGui.EthImGui_BeginPopupContextItem(id) != 0;
        public static bool BeginMenuBar() => NativeImGui.EthImGui_BeginMenuBar() != 0;
        public static void EndMenuBar() => NativeImGui.EthImGui_EndMenuBar();
        public static bool BeginMenu(string label) => NativeImGui.EthImGui_BeginMenu(label) != 0;
        public static void EndMenu() => NativeImGui.EthImGui_EndMenu();
        public static bool MenuItem(string label, string shortcut = null, bool selected = false, bool enabled = true)
            => NativeImGui.EthImGui_MenuItem(label, shortcut, selected ? 1 : 0, enabled ? 1 : 0) != 0;

        public static void GetCursorScreenPos(out float x, out float y) => NativeImGui.EthImGui_GetCursorScreenPos(out x, out y);
        public static void SetCursorScreenPos(float x, float y) => NativeImGui.EthImGui_SetCursorScreenPos(x, y);
        public static void GetCursorPos(out float x, out float y) => NativeImGui.EthImGui_GetCursorPos(out x, out y);
        public static void SetCursorPos(float x, float y) => NativeImGui.EthImGui_SetCursorPos(x, y);
        public static void GetContentRegionAvail(out float x, out float y) => NativeImGui.EthImGui_GetContentRegionAvail(out x, out y);
        public static void GetWindowPos(out float x, out float y) => NativeImGui.EthImGui_GetWindowPos(out x, out y);
        public static void GetWindowSize(out float x, out float y) => NativeImGui.EthImGui_GetWindowSize(out x, out y);
        public static void CalcTextSize(string text, out float w, out float h) => NativeImGui.EthImGui_CalcTextSize(text ?? "", out w, out h);
        public static float GetTextLineHeight() => NativeImGui.EthImGui_GetTextLineHeight();
        public static float GetFrameHeight() => NativeImGui.EthImGui_GetFrameHeight();
        public static void GetItemRectMin(out float x, out float y) => NativeImGui.EthImGui_GetItemRectMin(out x, out y);
        public static void GetItemRectMax(out float x, out float y) => NativeImGui.EthImGui_GetItemRectMax(out x, out y);
        public static void GetItemRectSize(out float x, out float y) => NativeImGui.EthImGui_GetItemRectSize(out x, out y);
        public static bool IsItemHovered() => NativeImGui.EthImGui_IsItemHovered() != 0;
        public static bool IsItemClicked(int button = 0) => NativeImGui.EthImGui_IsItemClicked(button) != 0;
        public static bool IsItemActive() => NativeImGui.EthImGui_IsItemActive() != 0;
        public static bool IsItemEdited() => NativeImGui.EthImGui_IsItemEdited() != 0;
        public static bool IsItemVisible() => NativeImGui.EthImGui_IsItemVisible() != 0;
        public static bool IsWindowHovered() => NativeImGui.EthImGui_IsWindowHovered() != 0;
        public static bool IsWindowFocused() => NativeImGui.EthImGui_IsWindowFocused() != 0;
        public static bool IsAnyItemActive() => NativeImGui.EthImGui_IsAnyItemActive() != 0;
        public static void SetItemDefaultFocus() => NativeImGui.EthImGui_SetItemDefaultFocus();
        public static void SetKeyboardFocusHere() => NativeImGui.EthImGui_SetKeyboardFocusHere();
        public static bool IsKeyDown(ImGuiKey key) => NativeImGui.EthImGui_IsKeyDown((int)key) != 0;
        public static void GetMousePos(out float x, out float y) => NativeImGui.EthImGui_GetMousePos(out x, out y);
        public static bool IsMouseDown(int button = 0) => NativeImGui.EthImGui_IsMouseDown(button) != 0;
        public static bool IsMouseClicked(int button = 0) => NativeImGui.EthImGui_IsMouseClicked(button) != 0;
        public static void GetIO(out float deltaTime, out float framerate, out float width, out float height)
            => NativeImGui.EthImGui_GetIO(out deltaTime, out framerate, out width, out height);
        public static bool WantCaptureMouse => NativeImGui.EthImGui_WantCaptureMouse() != 0;
        public static bool WantCaptureKeyboard => NativeImGui.EthImGui_WantCaptureKeyboard() != 0;
        public static void SetMouseCursor(ImGuiMouseCursor cursor) => NativeImGui.EthImGui_SetMouseCursor((int)cursor);

        // -------------------------------------------------------------- drawing
        public static void DrawLine(float x1, float y1, float x2, float y2, uint col, float thickness = 1.0f)
            => NativeImGui.EthImGui_DrawLine(x1, y1, x2, y2, col, thickness);
        public static void DrawRect(float x1, float y1, float x2, float y2, uint col, float rounding = 0, float thickness = 1.0f)
            => NativeImGui.EthImGui_DrawRect(x1, y1, x2, y2, col, rounding, thickness);
        public static void DrawRectFilled(float x1, float y1, float x2, float y2, uint col, float rounding = 0)
            => NativeImGui.EthImGui_DrawRectFilled(x1, y1, x2, y2, col, rounding);
        public static void DrawCircle(float cx, float cy, float radius, uint col, int segments = 0, float thickness = 1.0f)
            => NativeImGui.EthImGui_DrawCircle(cx, cy, radius, col, segments, thickness);
        public static void DrawCircleFilled(float cx, float cy, float radius, uint col, int segments = 0)
            => NativeImGui.EthImGui_DrawCircleFilled(cx, cy, radius, col, segments);
        public static void DrawText(float x, float y, uint col, string text)
            => NativeImGui.EthImGui_DrawText(x, y, col, text ?? "");
        public static void DrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, uint col, float thickness = 1.0f)
            => NativeImGui.EthImGui_DrawTriangle(x1, y1, x2, y2, x3, y3, col, thickness);
        public static void DrawTriangleFilled(float x1, float y1, float x2, float y2, float x3, float y3, uint col)
            => NativeImGui.EthImGui_DrawTriangleFilled(x1, y1, x2, y2, x3, y3, col);
        public static void DrawQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint col, float thickness = 1.0f)
            => NativeImGui.EthImGui_DrawQuad(x1, y1, x2, y2, x3, y3, x4, y4, col, thickness);
        public static void DrawQuadFilled(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint col)
            => NativeImGui.EthImGui_DrawQuadFilled(x1, y1, x2, y2, x3, y3, x4, y4, col);
    }

    /// <summary>Simple RGBA color helper.</summary>
    public struct ImVec4
    {
        public float X, Y, Z, W;
        public ImVec4(float x, float y, float z, float w) { X = x; Y = y; Z = z; W = w; }
        public uint ToU32() => ImGui.RGBA(X, Y, Z, W);
        public static ImVec4 FromU32(uint col)
        {
            return new ImVec4(
                ((col >> 0) & 0xFF) / 255.0f,
                ((col >> 8) & 0xFF) / 255.0f,
                ((col >> 16) & 0xFF) / 255.0f,
                ((col >> 24) & 0xFF) / 255.0f);
        }
    }
}
