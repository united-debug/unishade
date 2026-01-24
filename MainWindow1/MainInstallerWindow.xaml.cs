using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net.Http;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;

namespace UniShadeInstaller
{
    public partial class MainWindow : Window
    {
        // ================= CONFIGURATION =================
        private const string MainZipUrl = "https://github.com/united-debug/unishade/releases/download/shaders/UniShade.zip";
        private const string ModdedCrashHandlerUrl = "https://github.com/united-debug/unishade/releases/download/shaders/RobloxCrashHandler.exe";
        private const string CleanCrashHandlerUrl = "https://github.com/united-debug/unishade/releases/download/shaders/RobloxCrashHandler1.exe";

        private readonly HashSet<string> FilesToDelete = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "BX_XIV_Chromakeyplus.fx",
            "GrainSpread.fx",
            "ntsccUSTOM.FX",
            "NTSC_XOT.fx"
        };

        private readonly List<string> ShaderPackUrls = new List<string>
        {
            "https://github.com/crosire/reshade-shaders/tree/slim",
            "https://github.com/CeeJayDK/SweetFX",
            "https://github.com/crosire/reshade-shaders/tree/legacy",
            "https://github.com/FransBouma/OtisFX",
            "https://github.com/BlueSkyDefender/Depth3D",
            "https://github.com/luluco250/FXShaders",
            "https://github.com/Daodan317081/reshade-shaders",
            "https://github.com/brussell1/Shaders",
            "https://github.com/Fubaxiusz/fubax-shaders",
            "https://github.com/martymcmodding/qUINT",
            "https://github.com/AlucardDH/dh-reshade-shaders",
            "https://github.com/Radegast-FFXIV/Warp-FX",
            "https://github.com/prod80/prod80-ReShade-Repository",
            "https://github.com/originalnicodr/CorgiFX",
            "https://github.com/LordOfLunacy/Insane-Shaders",
            "https://github.com/LordKobra/CobraFX",
            "https://github.com/BlueSkyDefender/AstrayFX",
            "https://github.com/akgunter/crt-royale-reshade",
            "https://github.com/Matsilagi/RSRetroArch",
            "https://github.com/retroluxfilm/reshade-vrtoolkit",
            "https://github.com/AlexTuduran/FGFX",
            "https://github.com/papadanku/CShade",
            "https://github.com/EndlesslyFlowering/ReShade_HDR_shaders",
            "https://github.com/martymcmodding/iMMERSE",
            "https://github.com/vortigern11/vort_Shaders",
            "https://github.com/liuxd17thu/BX-Shade",
            "https://github.com/IAmTreyM/SHADERDECK",
            "https://github.com/martymcmodding/METEOR",
            "https://github.com/AnastasiaGals/Ann-ReShade",
            "https://github.com/Filoppi/PumboAutoHDR",
            "https://github.com/Zenteon/ZenteonFX",
            "https://github.com/Mortalitas/GShade-Shaders",
            "https://github.com/PthoEastCoast/Ptho-FX",
            "https://github.com/GimleLarpes/potatoFX",
            "https://github.com/nullfrctl/reshade-shaders",
            "https://github.com/MaxG2D/ReshadeSimpleHDRShaders",
            "https://github.com/BarbatosBachiko/Reshade-Shaders",
            "https://github.com/smolbbsoop/smolbbsoopshaders",
            "https://github.com/yplebedev/BFBFX",
            "https://github.com/outmode/rendepth-reshade",
            "https://github.com/P0NYSLAYSTATION/Scaling-Shaders",
            "https://github.com/umar-afzaal/LumeniteFX"
        };

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.LeftButton == MouseButtonState.Pressed) DragMove();
        }

        private void CloseBtn_Click(object sender, RoutedEventArgs e) => Application.Current.Shutdown();

        private void GoToInstallPage_Click(object sender, RoutedEventArgs e)
        {
            StartView.Visibility = Visibility.Collapsed;
            InstallView.Visibility = Visibility.Visible;
        }

        private void BackButton_Click(object sender, RoutedEventArgs e)
        {
            InstallView.Visibility = Visibility.Collapsed;
            StartView.Visibility = Visibility.Visible;
        }

        private async void InstallBtn_Click(object sender, RoutedEventArgs e)
        {
            RealInstallBtn.Visibility = Visibility.Collapsed;
            InstallProgressBar.Visibility = Visibility.Visible;
            PercentageText.Visibility = Visibility.Visible;
            StatusText.Text = "Initializing UniShade...";

            try
            {
                string roaming = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
                string uniShadeRoot = Path.Combine(roaming, "UniShade");
                string mainZipPath = Path.Combine(roaming, "temp_unishade.zip");

                // 1. Clean previous installation
                if (Directory.Exists(uniShadeRoot))
                    Directory.Delete(uniShadeRoot, true);

                // 2. Install Main UniShade Zip
                StatusText.Text = "Downloading Core UniShade...";
                await DownloadFileWithProgress(MainZipUrl, mainZipPath);

                StatusText.Text = "Extracting Core...";
                await Task.Run(() => ZipFile.ExtractToDirectory(mainZipPath, roaming));
                if (File.Exists(mainZipPath)) File.Delete(mainZipPath);

                // 3. Process Extra Shader Packs
                int totalPacks = ShaderPackUrls.Count;
                int currentPack = 0;

                string reshadeShadersPath = Path.Combine(uniShadeRoot, "reshade-shaders");
                string destinationShaders = Path.Combine(reshadeShadersPath, "Shaders");
                string destinationTextures = Path.Combine(reshadeShadersPath, "Textures");

                Directory.CreateDirectory(destinationShaders);
                Directory.CreateDirectory(destinationTextures);

                using (HttpClient client = new HttpClient())
                {
                    client.Timeout = TimeSpan.FromMinutes(5);

                    foreach (string repoUrl in ShaderPackUrls)
                    {
                        currentPack++;
                        //string progressPrefix = $"[{currentPack}/{totalPacks}]"; // Optional numbering

                        Dispatcher.Invoke(() => {
                            // Shows: Downloading: https://github.com/...
                            StatusText.Text = $"Downloading: {repoUrl}";
                            double percent = ((double)currentPack / totalPacks) * 100;
                            InstallProgressBar.Value = percent;
                            PercentageText.Text = $"{(int)percent}%";
                        });

                        try
                        {
                            await ProcessShaderPack(client, repoUrl, roaming, destinationShaders, destinationTextures);
                        }
                        catch (Exception ex)
                        {
                            Debug.WriteLine($"Failed to install pack {repoUrl}: {ex.Message}");
                        }
                    }
                }

                // 4. Remove Specific Bad Files
                StatusText.Text = "Removing Conflict Files...";
                await Task.Run(() => RemoveProblematicFiles(destinationShaders));

                // 5. Install Crash Handler
                StatusText.Text = "Finalizing...";
                await HandleCrashHandlers(isUninstalling: false);

                InstallView.Visibility = Visibility.Collapsed;
                FinishView.Visibility = Visibility.Visible;
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error during installation: " + ex.Message);
                RealInstallBtn.Visibility = Visibility.Visible;
                InstallProgressBar.Visibility = Visibility.Collapsed;
                PercentageText.Visibility = Visibility.Collapsed;
                StatusText.Text = "Installation Failed";
            }
        }

        private void RemoveProblematicFiles(string shadersFolder)
        {
            if (!Directory.Exists(shadersFolder)) return;

            var allFiles = Directory.GetFiles(shadersFolder, "*.*", SearchOption.AllDirectories);
            foreach (var file in allFiles)
            {
                if (FilesToDelete.Contains(Path.GetFileName(file)))
                {
                    try { File.Delete(file); } catch { }
                }
            }
        }

        private async Task ProcessShaderPack(HttpClient client, string repoUrl, string tempRoot, string destShaders, string destTextures)
        {
            string zipUrl;
            if (repoUrl.Contains("/tree/"))
                zipUrl = repoUrl.Replace("/tree/", "/archive/") + ".zip";
            else
                zipUrl = repoUrl.TrimEnd('/') + "/archive/HEAD.zip";

            string tempZipPath = Path.Combine(tempRoot, $"temp_pack_{Guid.NewGuid()}.zip");
            string tempExtractPath = Path.Combine(tempRoot, $"temp_extract_{Guid.NewGuid()}");

            try
            {
                byte[] data = await client.GetByteArrayAsync(zipUrl);
                await File.WriteAllBytesAsync(tempZipPath, data);

                ZipFile.ExtractToDirectory(tempZipPath, tempExtractPath);

                // Merge Shaders
                var foundShaderFolders = Directory.GetDirectories(tempExtractPath, "Shaders", SearchOption.AllDirectories)
                                                  .OrderBy(d => d.Length).ToList();
                List<string> processed = new List<string>();
                foreach (var folder in foundShaderFolders)
                {
                    if (processed.Any(p => folder.StartsWith(p))) continue;
                    MergeFolderContents(folder, destShaders);
                    processed.Add(folder);
                }

                // Merge Textures
                var foundTextureFolders = Directory.GetDirectories(tempExtractPath, "Textures", SearchOption.AllDirectories)
                                                   .OrderBy(d => d.Length).ToList();
                processed.Clear();
                foreach (var folder in foundTextureFolders)
                {
                    if (processed.Any(p => folder.StartsWith(p))) continue;
                    MergeFolderContents(folder, destTextures);
                    processed.Add(folder);
                }
            }
            finally
            {
                if (File.Exists(tempZipPath)) File.Delete(tempZipPath);
                if (Directory.Exists(tempExtractPath)) Directory.Delete(tempExtractPath, true);
            }
        }

        private static void MergeFolderContents(string sourcePath, string destinationPath)
        {
            DirectoryInfo sourceDir = new DirectoryInfo(sourcePath);

            foreach (FileInfo file in sourceDir.GetFiles())
            {
                string destFile = Path.Combine(destinationPath, file.Name);
                file.CopyTo(destFile, true);
            }

            foreach (DirectoryInfo subDir in sourceDir.GetDirectories())
            {
                string nextDestDir = Path.Combine(destinationPath, subDir.Name);
                Directory.CreateDirectory(nextDestDir);
                CopyRecursively(subDir.FullName, nextDestDir);
            }
        }

        private static void CopyRecursively(string sourcePath, string targetPath)
        {
            DirectoryInfo source = new DirectoryInfo(sourcePath);
            DirectoryInfo target = new DirectoryInfo(targetPath);

            foreach (FileInfo file in source.GetFiles())
            {
                file.CopyTo(Path.Combine(target.FullName, file.Name), true);
            }

            foreach (DirectoryInfo subDir in source.GetDirectories())
            {
                DirectoryInfo nextTargetSubDir = target.CreateSubdirectory(subDir.Name);
                CopyRecursively(subDir.FullName, nextTargetSubDir.FullName);
            }
        }

        private async void UninstallBtn_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string roaming = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
                string root = Path.Combine(roaming, "UniShade");
                if (Directory.Exists(root)) Directory.Delete(root, true);

                await HandleCrashHandlers(isUninstalling: true);

                MessageBox.Show("Uninstalled Successfully!");
                Application.Current.Shutdown();
            }
            catch (Exception ex) { MessageBox.Show("Uninstall Error: " + ex.Message); }
        }

        private async Task HandleCrashHandlers(bool isUninstalling)
        {
            string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            string url = isUninstalling ? CleanCrashHandlerUrl : ModdedCrashHandlerUrl;

            string[] basePaths = {
                Path.Combine(local, "Bloxstrap", "Versions"),
                Path.Combine(local, "Fishstrap", "Versions")
            };

            using (HttpClient client = new HttpClient())
            {
                try
                {
                    byte[] data = await client.GetByteArrayAsync(url);
                    foreach (var path in basePaths)
                    {
                        if (Directory.Exists(path))
                        {
                            string[] versionFolders = Directory.GetDirectories(path, "version-*");
                            foreach (var vFolder in versionFolders)
                            {
                                string targetFile = Path.Combine(vFolder, "RobloxCrashHandler.exe");
                                File.WriteAllBytes(targetFile, data);
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Debug.WriteLine("Handler error: " + ex.Message);
                }
            }
        }

        private async Task DownloadFileWithProgress(string url, string path)
        {
            using (HttpClient client = new HttpClient())
            using (var res = await client.GetAsync(url, HttpCompletionOption.ResponseHeadersRead))
            using (var s = await res.Content.ReadAsStreamAsync())
            using (var fs = new FileStream(path, FileMode.Create))
            {
                var total = res.Content.Headers.ContentLength ?? -1L;
                byte[] buf = new byte[8192];
                long read = 0; int r;
                while ((r = await s.ReadAsync(buf, 0, buf.Length)) > 0)
                {
                    await fs.WriteAsync(buf, 0, r); read += r;
                    if (total != -1)
                    {
                        double p = (double)read / total * 100;
                        Dispatcher.Invoke(() => {
                            InstallProgressBar.Value = p;
                            PercentageText.Text = $"{(int)p}%";
                        });
                    }
                }
            }
        }

        private void FinishBtn_Click(object sender, RoutedEventArgs e)
        {
            if (ChkRoblox.IsChecked == true) Process.Start(new ProcessStartInfo("https://www.roblox.com/users/7186211615/profile") { UseShellExecute = true });
            if (ChkYoutube.IsChecked == true) Process.Start(new ProcessStartInfo("https://www.youtube.com/@united.mm2") { UseShellExecute = true });
            if (ChkDiscord.IsChecked == true) Process.Start(new ProcessStartInfo("https://discord.gg/48n3khEb") { UseShellExecute = true });

            // New Checkbox Logic
            if (ChkSource.IsChecked == true) Process.Start(new ProcessStartInfo("https://github.com/united-debug/unishade") { UseShellExecute = true });

            Application.Current.Shutdown();
        }
    }
}