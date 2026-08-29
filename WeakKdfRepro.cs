// mcs -out:WeakKdfRepro.exe -r:System.Windows.Forms -r:System.Drawing WeakKdfRepro.cs
// mono WeakKdfRepro.exe
using System;
using System.Drawing;
using System.Reflection;
using System.Windows.Forms;

class WeakKdfRepro
{
    static void Main ()
    {
        Type e = Type.GetType("System.Windows.Forms.ThemeEngine, System.Windows.Forms");
        object t = e.GetProperty("Current", BindingFlags.Static | BindingFlags.Public).GetValue(null, null);
        t.GetType().BaseType.GetField("default_font", BindingFlags.Instance | BindingFlags.NonPublic).SetValue(t, new Font ("Noto Sans", 8.25f));

        MessageBox.Show("a");
    }
}
