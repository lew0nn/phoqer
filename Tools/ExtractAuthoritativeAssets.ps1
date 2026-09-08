param([string]$SourceRoot = "$PSScriptRoot\..\resources\ui\PHOQER\Sources", [string]$OutputRoot = "$PSScriptRoot\..\resources\ui\PHOQER")
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
public static class AssetCrop {
 public static Bitmap Crop(string path,int x,int y,int w,int h) {
  using(var src=new Bitmap(path)) return src.Clone(new Rectangle(x,y,w,h),PixelFormat.Format32bppArgb);
 }
 public static void Pointer(Bitmap b) {
  int cx=b.Width==423?218:(b.Width==309?158:192);
  // Local adjacent brushed-metal pixels, feathered only across the baked mark.
  int top=b.Width==423?91:(b.Width==309?58:77), bottom=b.Width==423?134:(b.Width==309?98:116);
  using(var original=(Bitmap)b.Clone())
  for(int y=top;y<bottom;y++)
   for(int x=cx-10;x<=cx+10;x++) {
    var l=original.GetPixel(cx-13,y); var q=original.GetPixel(cx+13,y);
    double t=(x-cx+10)/20.0;
    double a=Math.Min(1,Math.Min((10-Math.Abs(x-cx))/3.0,Math.Min((y-top)/3.0,(bottom-1-y)/3.0)));
    var o=original.GetPixel(x,y);
    b.SetPixel(x,y,Color.FromArgb(255,(int)(o.R+(l.R+(q.R-l.R)*t-o.R)*a),(int)(o.G+(l.G+(q.G-l.G)*t-o.G)*a),(int)(o.B+(l.B+(q.B-l.B)*t-o.B)*a)));
   }
 }
 public static void EmptyTrack(Bitmap b,int x,int y,int w,int h,int sampleY) {
  // Clone the existing dark recessed material; preserve all outer hardware.
  using(var original=(Bitmap)b.Clone())
   for(int yy=y;yy<y+h;yy++) for(int xx=x;xx<x+w;xx++)
    b.SetPixel(xx,yy,original.GetPixel(xx,sampleY+(yy-y)%28));
 }
 public static void Meter(Bitmap b,bool lit) {
  using(var original=(Bitmap)b.Clone()) {
   // 21 source segments, native 13px pitch, sampled from original lit/unlit cells.
   for(int y=9;y<35;y++) for(int x=15;x<295;x++)
    b.SetPixel(x,y,original.GetPixel((lit?15:275)+(x-15)%13,y));
  }
 }
 public static void EmptyWave(Bitmap b) {
  using(var original=(Bitmap)b.Clone())
   for(int y=5;y<b.Height-5;y++) for(int x=6;x<b.Width-6;x++)
    b.SetPixel(x,y,original.GetPixel(12+x%7,9+y%3));
 }
}
'@
$sheets=@('ChatGPT Image Sep 5, 2026, 04_33_55 PM (1).png','ChatGPT Image Sep 5, 2026, 04_33_55 PM (2).png','ChatGPT Image Sep 5, 2026, 04_33_55 PM (3).png','ChatGPT Image Sep 5, 2026, 04_33_56 PM (4).png','ChatGPT Image Sep 5, 2026, 04_33_56 PM (5).png','ChatGPT Image Sep 5, 2026, 04_33_56 PM (6).png','ChatGPT Image Sep 5, 2026, 04_34_38 PM.png')
$rows=[Collections.Generic.List[string]]::new()
function Export-Crop($Name,$Sheet,$Rect,$Purpose,$Cleanup='') {
 $path=Join-Path $OutputRoot $Name
 [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))
 $b=[AssetCrop]::Crop((Join-Path $SourceRoot $sheets[$Sheet]),$Rect[0],$Rect[1],$Rect[2],$Rect[3])
 switch($Cleanup) {
  'pointer' {[AssetCrop]::Pointer($b)}
  'purpletrack' {[AssetCrop]::EmptyTrack($b,101,70,72,766,85)}
  'icetrack' {[AssetCrop]::EmptyTrack($b,80,82,80,735,90)}
  'redtrack' {[AssetCrop]::EmptyTrack($b,82,82,80,735,90)}
  'wave' {[AssetCrop]::EmptyWave($b)}
  'meter_off' {[AssetCrop]::Meter($b,$false)}
  'meter_on' {[AssetCrop]::Meter($b,$true)}
 }
 $b.Save($path,[Drawing.Imaging.ImageFormat]::Png); $b.Dispose()
 $rows.Add("| $Name | $($sheets[$Sheet]) | $($Rect -join ', ') | $Purpose; $Cleanup |")
}
Export-Crop 'Knobs/Purple/knob_body_purple.png' 0 @(197,581,423,423) 'Default knob, neutral state; feathered adjacent-pixel pointer cleanup' 'pointer'
Export-Crop 'Knobs/Ice/knob_body_ice.png' 1 @(23,195,309,309) 'Synth knob, unlit state' 'pointer'
Export-Crop 'Knobs/Red/knob_body_red.png' 2 @(55,101,384,384) 'Burp knob, unlit state' 'pointer'
Export-Crop 'Tide/Purple/tide_body_purple.png' 3 @(910,91,280,909) 'Empty industrial housing; remove preview column with recessed source material' 'purpletrack'
Export-Crop 'Tide/Purple/tide_track_purple.png' 3 @(1013,172,67,100) 'Unlit recessed track texture'
Export-Crop 'Tide/Purple/tide_fill_purple.png' 3 @(448,173,111,733) 'Luminous column, clipped at runtime'
Export-Crop 'Tide/Purple/tide_handle_purple.png' 3 @(647,444,205,166) 'Single detached hardware handle'
Export-Crop 'Tide/Ice/tide_body_ice.png' 4 @(429,91,244,895) 'Ice housing with baked meter column removed by adjacent recessed material' 'icetrack'
Export-Crop 'Tide/Ice/tide_track_ice.png' 4 @(511,180,76,100) 'Unlit ice recess'
Export-Crop 'Tide/Ice/tide_fill_ice.png' 4 @(511,349,76,565) 'Source luminous ice column'
Export-Crop 'Tide/Ice/tide_handle_ice.png' 4 @(142,190,122,72) 'Single ice handle from slider'
Export-Crop 'Tide/Red/tide_body_red.png' 4 @(1121,91,245,895) 'Red housing with baked meter column removed by adjacent recessed material' 'redtrack'
Export-Crop 'Tide/Red/tide_track_red.png' 4 @(1204,180,76,100) 'Unlit red recess'
Export-Crop 'Tide/Red/tide_fill_red.png' 4 @(1204,349,76,565) 'Source luminous red column'
Export-Crop 'Tide/Red/tide_handle_red.png' 4 @(835,190,122,72) 'Single red handle from slider'
$buttons=@('voice','formant','harmonics','space','bite')
for($i=0;$i -lt 5;$i++){Export-Crop "Buttons/button_$($buttons[$i]).png" 6 @((39+137*$i),861,105,104) 'Exact developer-map hardware sidebar button'}
$colors=@('purple','ice','red')
for($i=0;$i -lt 3;$i++){
 $c=$colors[$i]
 Export-Crop "SmallParts/led_$c.png" 5 @(774,(63+123*$i),103,104) 'Supplied active LED'
 $meterY=@(497,585,675)[$i]
 Export-Crop "Displays/meter_off_$c.png" 6 @(789,$meterY,305,60) 'Housing, scale, original unlit cells replicated across strip' 'meter_off'
 Export-Crop "Displays/meter_on_$c.png" 6 @(789,$meterY,305,60) 'Original lit cells replicated; reveal only live level' 'meter_on'
 Export-Crop "Displays/wave_frame_$c.png" 6 @(1149,$meterY,260,59) 'Original display frame, baked trace removed with clean interior pixels' 'wave'
}
Export-Crop 'SmallParts/led_off.png' 6 @(1080,861,36,37) 'Supplied unlit LED'
Export-Crop 'SmallParts/menu_icon.png' 6 @(1210,857,53,47) 'Supplied menu hardware'
Export-Crop 'SmallParts/vent_grille.png' 6 @(1310,853,94,59) 'Supplied vent, reserved'
Export-Crop 'SmallParts/screw.png' 6 @(1341,964,40,42) 'Supplied screw, reserved'
$manifest=@('# Authoritative PHOQER production asset manifest','','All crop bounds are x, y, width, height in native source pixels. Originals are preserved in Sources and never embedded. Offline cleanup clones original local material; no generated artwork. Runtime embeds only the production derivatives.','','| Output | Source sheet | Crop bounds | Purpose / cleanup |','|---|---|---|---|')+$rows+@('','','Approved seal resources remain ../Purple/purple_seal.png, ../Red/red_seal.png, ../Ice/ice_seal.png unchanged. Their source frame is the sole portrait frame.','The common developer-map buttons retain their original material colors. The selected LED follows the sound identity.','Meter derivatives share unchanged housing/scale; runtime clips the lit derivative to whole native segment cells. Wave-frame interiors contain no baked waveform.')
[IO.File]::WriteAllLines((Join-Path $OutputRoot 'ASSET_MANIFEST.md'),$manifest)
[void][IO.Directory]::CreateDirectory((Join-Path $OutputRoot 'Sources'))
foreach($s in $sheets){
 $from=[IO.Path]::GetFullPath((Join-Path $SourceRoot $s))
 $to=[IO.Path]::GetFullPath((Join-Path (Join-Path $OutputRoot 'Sources') $s))
 if($from -ne $to){Copy-Item -LiteralPath $from -Destination $to -Force}
}
Write-Output "Extracted $($rows.Count) production PNG assets."
