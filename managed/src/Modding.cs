// ===========================================================================
//  EthhhSDK.Managed - public modding API (IEthMod, settings, logging, sdk)
// ===========================================================================
using System;
using System.Globalization;

namespace EthhhSDK.Modding
{
    /// <summary>
    /// Implement this interface in your mod assembly. The host creates one
    /// instance per mod and calls the callbacks from the game's logic/render
    /// thread.
    /// </summary>
    public interface IEthMod
    {
        void OnInitialize();
        void OnTick(int tickIndex);
        void OnGui();
        void OnOverlayChanged(bool open);
    }

    /// <summary>Convenience base class with empty implementations.</summary>
    public interface IEthModLifecycle
    {
        void OnEnabledChanged(bool enabled);
    }

    public abstract class EthModBase : IEthMod, IEthModLifecycle
    {
        public virtual void OnInitialize() { }
        public virtual void OnTick(int tickIndex) { }
        public virtual void OnGui() { }
        public virtual void OnOverlayChanged(bool open) { }
        public virtual void OnEnabledChanged(bool enabled) { }
    }

    /// <summary>Identifies the mod whose callback is currently running.</summary>
    public static class ModContext
    {
        public static string ModId { get; internal set; } = string.Empty;
        public static string ModDir { get; internal set; } = string.Empty;
    }

    /// <summary>Per-mod persistent settings (stored in EthhhSDK/config/mod_settings.ini).</summary>
    public static class ModSettings
    {
        private static string Id(string explicitId) => string.IsNullOrEmpty(explicitId) ? ModContext.ModId : explicitId;

        public static string GetString(string key, string def = "", string explicitId = null)
        {
            byte[] buffer = new byte[2048];
            if (Native.EthMod_GetSetting(Id(explicitId), key, buffer, buffer.Length) == 0) return def;
            int len = Array.IndexOf(buffer, (byte)0);
            if (len < 0) len = buffer.Length;
            string value = System.Text.Encoding.UTF8.GetString(buffer, 0, len);
            return string.IsNullOrEmpty(value) ? def : value;
        }

        public static void SetString(string key, string value, string explicitId = null)
            => Native.EthMod_SetSetting(Id(explicitId), key, value ?? string.Empty);

        public static float GetFloat(string key, float def = 0.0f, string explicitId = null)
            => Native.EthMod_GetSettingFloat(Id(explicitId), key, def);

        public static void SetFloat(string key, float value, string explicitId = null)
            => Native.EthMod_SetSettingFloat(Id(explicitId), key, value);

        public static int GetInt(string key, int def = 0, string explicitId = null)
        {
            string text = GetString(key, null, explicitId);
            if (string.IsNullOrEmpty(text)) return def;
            return int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out int value) ? value : def;
        }

        public static void SetInt(string key, int value, string explicitId = null)
            => SetString(key, value.ToString(CultureInfo.InvariantCulture), explicitId);

        public static bool GetBool(string key, bool def = false, string explicitId = null)
            => Native.EthMod_GetSettingBool(Id(explicitId), key, def ? 1 : 0) != 0;

        public static void SetBool(string key, bool value, string explicitId = null)
            => Native.EthMod_SetSettingBool(Id(explicitId), key, value ? 1 : 0);

        public static void Save() => Native.EthMod_SaveSettings();
    }

    /// <summary>Logging into the SDK console / log file.</summary>
    public static class ModLog
    {
        public static void Debug(string message) => Native.EthLog_Write(0, Prefix(message));
        public static void Info(string message) => Native.EthLog_Write(1, Prefix(message));
        public static void Warn(string message) => Native.EthLog_Write(2, Prefix(message));
        public static void Error(string message) => Native.EthLog_Write(3, Prefix(message));

        private static string Prefix(string message)
        {
            string id = ModContext.ModId;
            return string.IsNullOrEmpty(id) ? message ?? "" : "[" + id + "] " + (message ?? "");
        }
    }

    public static class ModStatus
    {
        public static bool IsEnabled => Native.EthMod_IsEnabled(ModContext.ModId) != 0;
        /// <summary>Shows an error in the mod manager and disables the mod.</summary>
        public static void ReportError(string message) => Native.EthMod_ReportError(ModContext.ModId, message ?? "unknown error");
        public static void Set(string status) => Native.EthMod_SetStatus(ModContext.ModId, status ?? "");
        public static void OpenFolder() => Native.EthMod_OpenFolder();
    }
}

namespace EthhhSDK
{
    /// <summary>SDK information.</summary>
    public static class Sdk
    {
        public static string Version => MarshalString(Native.EthSDK_Version());
        public static string BuildDate => MarshalString(Native.EthSDK_BuildDate());
        public static string GameDir => MarshalString(Native.EthSDK_GameDir());
        public static string ModsDir => MarshalString(Native.EthSDK_ModsDir());
        public static string ConfigDir => MarshalString(Native.EthSDK_ConfigDir());
        public static bool IsGameHooked => Native.EthHooks_GameHooked() != 0;

        private static string MarshalString(IntPtr ptr)
        {
            if (ptr == IntPtr.Zero) return string.Empty;
            int len = 0;
            while (System.Runtime.InteropServices.Marshal.ReadByte(ptr, len) != 0) len++;
            if (len == 0) return string.Empty;
            byte[] bytes = new byte[len];
            System.Runtime.InteropServices.Marshal.Copy(ptr, bytes, 0, len);
            return System.Text.Encoding.UTF8.GetString(bytes);
        }
    }
}
