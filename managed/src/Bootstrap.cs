// ===========================================================================
//  EthhhSDK.Managed - mod host bootstrap
//
//  Entry point for ICLRRuntimeHost::ExecuteInDefaultAppDomain:
//      EthhhSDK.Managed.Bootstrap.Initialize(string)
//
//  The native side passes:
//      <gameDir>\n
//      <modId>|<entryAssemblyPath>|<modDir>\n
//      ...
//
//  The host loads every mod assembly, instantiates its IEthMod implementation
//  and registers the three native callbacks (tick / gui / overlay).
// ===========================================================================
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using EthhhSDK.Modding;

namespace EthhhSDK.Managed
{
    public static class Bootstrap
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void TickDelegate(int tickIndex);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void GuiDelegate();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void OverlayDelegate(int open);

        // Keep the delegates rooted for the lifetime of the process: the native
        // side only stores raw function pointers.
        private static readonly TickDelegate s_tick = OnNativeTick;
        private static readonly GuiDelegate s_gui = OnNativeGui;
        private static readonly OverlayDelegate s_overlay = OnNativeOverlay;

        private sealed class ModEntry
        {
            public string Id;
            public string Dir;
            public string Name;
            public IEthMod Instance;
            public bool Alive = true;
            public bool Enabled = true;
        }

        private static readonly List<ModEntry> s_mods = new List<ModEntry>();
        private static bool s_initialized;
        private static bool s_callbacksSet;

        public static int Initialize(string argument)
        {
            if (s_initialized)
                return 0;
            s_initialized = true;

            AppDomain.CurrentDomain.AssemblyResolve += ResolveHostAssembly;

            string text = argument ?? string.Empty;
            string[] lines = text.Replace("\r", string.Empty).Split('\n');
            if (lines.Length == 0)
                return 1;

            string gameDir = lines[0].Trim();
            if (gameDir.Length == 0)
                gameDir = AppDomain.CurrentDomain.BaseDirectory;

            Native.EthLog_Write(1, "managed: Bootstrap.Initialize, game dir " + gameDir);

            for (int i = 1; i < lines.Length; i++)
            {
                string line = lines[i].Trim();
                if (line.Length == 0) continue;

                string[] parts = line.Split('|');
                if (parts.Length < 3)
                {
                    Native.EthLog_Write(2, "managed: malformed mod line: " + line);
                    continue;
                }

                string id = parts[0];
                string entry = parts[1];
                string dir = parts[2];
                LoadMod(id, entry, dir);
            }

            if (s_mods.Count > 0)
            {
                Native.EthHooks_SetManagedCallbacks(
                    Marshal.GetFunctionPointerForDelegate(s_tick),
                    Marshal.GetFunctionPointerForDelegate(s_gui),
                    Marshal.GetFunctionPointerForDelegate(s_overlay));
                s_callbacksSet = true;
            }

            Native.EthLog_Write(1, "managed: " + s_mods.Count + " mod(s) initialised, callbacks " +
                                   (s_callbacksSet ? "registered" : "not registered"));
            return 0;
        }

        // ------------------------------------------------------------------ load
        private static void LoadMod(string id, string entry, string dir)
        {
            var info = new ModEntry { Id = id, Dir = dir, Name = ReadDisplayName(dir) };
            try
            {
                if (!File.Exists(entry))
                    throw new FileNotFoundException("mod assembly not found", entry);

                Assembly assembly = Assembly.LoadFrom(entry);
                Type type = FindModType(assembly);
                if (type == null)
                    throw new InvalidOperationException("no class implementing EthhhSDK.Modding.IEthMod found");

                var instance = (IEthMod)Activator.CreateInstance(type);
                info.Instance = instance;

                ModContext.ModId = id;
                ModContext.ModDir = dir;
                instance.OnInitialize();
                ModContext.ModId = string.Empty;
                ModContext.ModDir = string.Empty;

                s_mods.Add(info);
                ModLog.Info("managed mod loaded (" + type.FullName + ")");
                Native.EthMod_SetStatus(id, "loaded (managed)");
            }
            catch (Exception ex)
            {
                ModContext.ModId = string.Empty;
                ModContext.ModDir = string.Empty;
                string message = Flatten(ex);
                Native.EthMod_ReportError(id, message);
                Native.EthLog_Write(3, "managed: failed to load " + id + ": " + message);
            }
        }

        private static string ReadDisplayName(string dir)
        {
            try
            {
                string manifest = Path.Combine(dir, "mod.json");
                if (!File.Exists(manifest)) return Path.GetFileName(dir);
                string text = File.ReadAllText(manifest, Encoding.UTF8);
                Match m = Regex.Match(text, "\"name\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"");
                if (m.Success) return Regex.Unescape(m.Groups[1].Value);
            }
            catch { }
            return Path.GetFileName(dir);
        }

        private static Type FindModType(Assembly assembly)
        {
            Type[] types;
            try { types = assembly.GetTypes(); }
            catch (ReflectionTypeLoadException ex)
            {
                var loaded = new List<Type>();
                foreach (Type t in ex.Types) if (t != null) loaded.Add(t);
                types = loaded.ToArray();
            }

            foreach (Type t in types)
            {
                if (!t.IsClass || t.IsAbstract) continue;
                if (!typeof(IEthMod).IsAssignableFrom(t)) continue;
                if (t.GetConstructor(Type.EmptyTypes) == null) continue;
                return t;
            }
            return null;
        }

        private static Assembly ResolveHostAssembly(object sender, ResolveEventArgs args)
        {
            try
            {
                string simpleName = new AssemblyName(args.Name).Name;
                if (string.Equals(simpleName, "EthhhSDK.Managed", StringComparison.OrdinalIgnoreCase))
                    return typeof(Bootstrap).Assembly;

                string selfDir = Path.GetDirectoryName(typeof(Bootstrap).Assembly.Location);
                string candidate = Path.Combine(selfDir ?? string.Empty, simpleName + ".dll");
                if (File.Exists(candidate))
                    return Assembly.LoadFrom(candidate);
            }
            catch { }
            return null;
        }

        // ------------------------------------------------------------- callbacks
        private static void OnNativeTick(int tickIndex)
        {
            for (int i = 0; i < s_mods.Count; i++)
            {
                ModEntry mod = s_mods[i];
                if (!ShouldInvoke(mod)) continue;
                SetContext(mod);
                try { mod.Instance.OnTick(tickIndex); }
                catch (Exception ex) { Disable(mod, "OnTick", ex); }
            }
            ClearContext();
        }

        private static void OnNativeGui()
        {
            for (int i = 0; i < s_mods.Count; i++)
            {
                ModEntry mod = s_mods[i];
                if (!ShouldInvoke(mod)) continue;
                SetContext(mod);
                try
                {
                    ImGui.PushID(mod.Id);
                    try
                    {
                        if (ImGui.CollapsingHeader(mod.Name))
                        {
                            ImGui.PushStyleVar(ImGuiStyleVar.ItemSpacing, 8.0f, 6.0f);
                            try { mod.Instance.OnGui(); }
                            finally { ImGui.PopStyleVar(1); }
                        }
                    }
                    finally { ImGui.PopID(); }
                }
                catch (Exception ex) { Disable(mod, "OnGui", ex); }
            }
            ClearContext();
        }

        private static void OnNativeOverlay(int open)
        {
            for (int i = 0; i < s_mods.Count; i++)
            {
                ModEntry mod = s_mods[i];
                if (!ShouldInvoke(mod)) continue;
                SetContext(mod);
                try { mod.Instance.OnOverlayChanged(open != 0); }
                catch (Exception ex) { Disable(mod, "OnOverlayChanged", ex); }
            }
            ClearContext();
        }

        private static void SetContext(ModEntry mod)
        {
            ModContext.ModId = mod.Id;
            ModContext.ModDir = mod.Dir;
        }

        private static bool ShouldInvoke(ModEntry mod)
        {
            if (!mod.Alive) return false;
            SetContext(mod);
            bool enabled = Native.EthMod_IsEnabled(mod.Id) != 0;
            if (enabled != mod.Enabled)
            {
                mod.Enabled = enabled;
                // Assemblies stay loaded until restart; callbacks stop now.
                if (mod.Instance is IEthModLifecycle lifecycle)
                {
                    try { lifecycle.OnEnabledChanged(enabled); }
                    catch (Exception ex) { Disable(mod, "OnEnabledChanged", ex); }
                }
            }
            return enabled && mod.Alive;
        }

        private static void ClearContext()
        {
            ModContext.ModId = string.Empty;
            ModContext.ModDir = string.Empty;
        }

        private static void Disable(ModEntry mod, string where, Exception ex)
        {
            mod.Alive = false;
            if (mod.Instance is IEthModLifecycle lifecycle)
            {
                try { lifecycle.OnEnabledChanged(false); }
                catch { }
            }
            string message = where + ": " + Flatten(ex);
            Native.EthMod_ReportError(mod.Id, message);
            Native.EthLog_Write(3, "managed: " + mod.Id + " " + message);
        }

        private static string Flatten(Exception ex)
        {
            if (ex is TargetInvocationException && ex.InnerException != null)
                ex = ex.InnerException;
            var sb = new StringBuilder(ex.GetType().Name + ": " + ex.Message);
            Exception inner = ex.InnerException;
            int depth = 0;
            while (inner != null && depth++ < 4)
            {
                sb.Append(" -> ").Append(inner.GetType().Name).Append(": ").Append(inner.Message);
                inner = inner.InnerException;
            }
            return sb.ToString();
        }
    }

    /// <summary>Signature-friendly alias for Bootstrap.Initialize (native uses Bootstrap.Initialize).</summary>
    public static class Host
    {
        public static int Initialize(string argument) => Bootstrap.Initialize(argument);
    }
}
