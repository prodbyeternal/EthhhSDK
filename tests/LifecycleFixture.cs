using System;
using EthhhSDK.Modding;

public sealed class LifecycleFixture : EthModBase
{
    public static int Ticks, Guis, Overlays, Disabled, Enabled;
    public static bool ThrowGui;
    public override void OnTick(int tickIndex) { Ticks++; }
    public override void OnGui() { Guis++; if (ThrowGui) throw new InvalidOperationException("fixture failure"); }
    public override void OnOverlayChanged(bool open) { Overlays++; }
    public override void OnEnabledChanged(bool enabled) { if(enabled) Enabled++; else Disabled++; }
}
