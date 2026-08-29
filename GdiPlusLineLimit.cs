// Does GDI+ draw anything when StringFormatFlags.LineLimit is set and the
// layout rectangle is shorter than one line? Run this on Windows to find out.
//
// Build with the in-box compiler (no install needed):
//     %WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe /r:System.Drawing.dll GdiPlusLineLimit.cs
//     GdiPlusLineLimit.exe
//
// It draws a button label the way System.Windows.Forms.ThemeWin32Classic
// does and counts the pixels that ended up on the surface, for a range of
// rectangle heights, with and without LineLimit.

using System;
using System.Drawing;
using System.Drawing.Imaging;

class GdiPlusLineLimit
{
    const int Width = 86;      // MessageBox button size
    const int Height = 80;     // tall enough for the three line case below
    static readonly Color Background = Color.FromArgb(255, 236, 233, 216);

    static int Ink(Font font, StringFormatFlags flags, float rectHeight)
    {
        return Ink(font, flags, rectHeight, "OK");
    }

    static int Ink(Font font, StringFormatFlags flags, float rectHeight, string text)
    {
        using (Bitmap bmp = new Bitmap(Width, Height, PixelFormat.Format32bppRgb))
        {
            using (Graphics g = Graphics.FromImage(bmp))
            using (StringFormat sf = new StringFormat(flags))
            using (Brush brush = new SolidBrush(Color.Black))
            {
                g.Clear(Background);
                // ButtonBase's text_format.
                sf.Alignment = StringAlignment.Center;
                sf.LineAlignment = StringAlignment.Center;
                sf.HotkeyPrefix = System.Drawing.Text.HotkeyPrefix.Show;
                // Rectangle.Inflate (ClientRectangle, -4, -4) of an 86x23 button.
                g.DrawString(text, font, brush, new RectangleF(4, 4, 78, rectHeight), sf);
            }

            int ink = 0;
            int background = Background.ToArgb() & 0xFFFFFF;
            for (int y = 0; y < Height; y++)
                for (int x = 0; x < Width; x++)
                    if ((bmp.GetPixel(x, y).ToArgb() & 0xFFFFFF) != background)
                        ink++;
            return ink;
        }
    }

    static void Main()
    {
        using (Bitmap probe = new Bitmap(1, 1))
        using (Graphics g = Graphics.FromImage(probe))
        using (Font font = new Font(FontFamily.GenericSansSerif, 8.25f, FontStyle.Regular))
        using (StringFormat sf = new StringFormat(StringFormatFlags.LineLimit))
        {
            FontFamily ff = font.FontFamily;
            Console.WriteLine("font                 : {0} {1}pt", font.Name, font.SizeInPoints);
            Console.WriteLine("Font.Height          : {0}", font.Height);
            Console.WriteLine("font.GetHeight(g)    : {0:F4}", font.GetHeight(g));
            Console.WriteLine("em/lineSpacing/asc/desc: {0}/{1}/{2}/{3}",
                ff.GetEmHeight(FontStyle.Regular), ff.GetLineSpacing(FontStyle.Regular),
                ff.GetCellAscent(FontStyle.Regular), ff.GetCellDescent(FontStyle.Regular));
            Console.WriteLine("MeasureString height : {0:F4}",
                g.MeasureString("OK", font, new SizeF(78, 15), sf).Height);
            Console.WriteLine();

            Console.WriteLine("one line, \"OK\"");
            Console.WriteLine("{0,-8}{1,-12}{2,-12}", "rectH", "no flags", "LineLimit");
            float[] heights = { 30, 20, 18, 17, 16, 15, 14, 13, 12, 11, 10 };
            foreach (float h in heights)
            {
                Console.WriteLine("{0,-8}{1,-12}{2,-12}", h,
                    Ink(font, 0, h),
                    Ink(font, StringFormatFlags.LineLimit, h));
            }

            // Does LineLimit still drop the lines after the first one?
            Console.WriteLine();
            Console.WriteLine("three lines, \"A\\nB\\nC\"");
            Console.WriteLine("{0,-8}{1,-12}{2,-12}", "rectH", "no flags", "LineLimit");
            float[] tall = { 60, 45, 42, 30, 28, 20, 15, 14, 13, 12, 10 };
            foreach (float h in tall)
            {
                Console.WriteLine("{0,-8}{1,-12}{2,-12}", h,
                    Ink(font, 0, h, "A\nB\nC"),
                    Ink(font, StringFormatFlags.LineLimit, h, "A\nB\nC"));
            }
        }
    }
}
