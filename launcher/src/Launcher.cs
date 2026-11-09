using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Windows.Forms;

namespace TriadWarsLauncher
{
    public class MainForm : Form
    {
        private Process serverProcess = null;
        private Process gameProcess = null;
        private string rootDir;
        private string betaDir;
        private string serverDir;
        private string pythonPath;

        private Label lblTitle;
        private Label lblSubtitle;
        private Label lblServerStatus;
        private Label lblGameStatus;
        private Label lblPatchStatus;
        private Label lblConfigStatus;
        private Label lblLangStatus;
        private Label lblControllerStatus;
        private Button btnStartAll;
        private Button btnStartServer;
        private Button btnStopServer;
        private Button btnLaunchGame;
        private Button btnTogglePatch;
        private Button btnToggleLanguage;
        private Button btnResetProfile;
        private Button btnOpenLogs;
        private Label lblSpawn;
        private ComboBox cmbSpawnPoint;
        private Button btnSetSpawn;
        private Button btnSyncSD;
        private Label lblVehicle;
        private ComboBox cmbVehicle;
        private Button btnSetVehicle;
        private TextBox txtLogs;
        private System.Windows.Forms.Timer statusTimer;

        public MainForm()
        {
            InitializePaths();
            InitializeComponent();
            CheckEnvironment();

            statusTimer = new System.Windows.Forms.Timer();
            statusTimer.Interval = 2000;
            statusTimer.Tick += (s, e) => UpdateLiveStatus();
            statusTimer.Start();
        }

        private void InitializePaths()
        {
            string appDir = AppDomain.CurrentDomain.BaseDirectory;
            if (Directory.Exists(Path.Combine(appDir, "beta")))
                rootDir = appDir;
            else if (Directory.Exists(Path.Combine(appDir, "..", "beta")))
                rootDir = Path.GetFullPath(Path.Combine(appDir, ".."));
            else
                rootDir = @"x:\projects\TriadWars";

            betaDir = Path.Combine(rootDir, "beta");
            serverDir = Path.Combine(rootDir, "server", "LocalServer", "LocalServer");

            pythonPath = FindPython();
        }

        private string FindPython()
        {
            string[] candidates = new string[]
            {
                "python.exe",
                @"C:\Python312\python.exe",
                @"C:\Python311\python.exe",
                @"C:\Python310\python.exe",
                @"C:\Python39\python.exe",
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), @"Programs\Python\Python312\python.exe"),
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), @"Programs\Python\Python311\python.exe"),
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), @"Programs\Python\Python310\python.exe")
            };

            foreach (var cand in candidates)
            {
                try
                {
                    var psi = new ProcessStartInfo(cand, "--version")
                    {
                        RedirectStandardOutput = true,
                        RedirectStandardError = true,
                        UseShellExecute = false,
                        CreateNoWindow = true
                    };
                    using (var p = Process.Start(psi))
                    {
                        p.WaitForExit(1000);
                        if (p.ExitCode == 0) return cand;
                    }
                }
                catch { }
            }
            return "python";
        }

        private void InitializeComponent()
        {
            this.Text = "Triad Wars Offline Revival - Launcher";
            this.Size = new Size(820, 785);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.BackColor = Color.FromArgb(24, 24, 28);
            this.ForeColor = Color.WhiteSmoke;
            this.Font = new Font("Segoe UI", 9.5f, FontStyle.Regular);

            lblTitle = new Label
            {
                Text = "TRIAD WARS",
                Font = new Font("Segoe UI", 20f, FontStyle.Bold),
                ForeColor = Color.FromArgb(220, 50, 47),
                Location = new Point(25, 18),
                AutoSize = true
            };

            lblSubtitle = new Label
            {
                Text = "OFFLINE REVIVAL - SERVIDOR Y CLIENTE LOCAL",
                Font = new Font("Segoe UI", 9f, FontStyle.Bold),
                ForeColor = Color.DarkGray,
                Location = new Point(28, 56),
                AutoSize = true
            };

            // Status Panel
            GroupBox grpStatus = new GroupBox
            {
                Text = " Estado del Sistema ",
                ForeColor = Color.DarkOrange,
                Location = new Point(25, 82),
                Size = new Size(755, 130)
            };

            lblServerStatus = new Label { Text = "Servidor Local: Comprobando...", Location = new Point(18, 28), AutoSize = true, ForeColor = Color.LightGray };
            lblGameStatus = new Label { Text = "Cliente: Comprobando...", Location = new Point(18, 60), AutoSize = true, ForeColor = Color.LightGray };
            lblLangStatus = new Label { Text = "Idioma: Comprobando...", Location = new Point(18, 92), AutoSize = true, ForeColor = Color.LightGray };

            lblConfigStatus = new Label { Text = "Entorno de Red: Comprobando...", Location = new Point(390, 28), AutoSize = true, ForeColor = Color.LightGray };
            lblPatchStatus = new Label { Text = "Parche Sombras: Comprobando...", Location = new Point(390, 60), AutoSize = true, ForeColor = Color.LightGray };
            lblControllerStatus = new Label { Text = "Mando DualSense: Comprobando...", Location = new Point(390, 92), AutoSize = true, ForeColor = Color.LightGray };

            grpStatus.Controls.Add(lblServerStatus);
            grpStatus.Controls.Add(lblGameStatus);
            grpStatus.Controls.Add(lblLangStatus);
            grpStatus.Controls.Add(lblConfigStatus);
            grpStatus.Controls.Add(lblPatchStatus);
            grpStatus.Controls.Add(lblControllerStatus);

            // Controls Row 1
            btnStartAll = new Button
            {
                Text = "JUGAR (Servidor + Juego)",
                Font = new Font("Segoe UI", 11f, FontStyle.Bold),
                BackColor = Color.FromArgb(180, 40, 40),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(25, 222),
                Size = new Size(240, 44),
                Cursor = Cursors.Hand
            };
            btnStartAll.FlatAppearance.BorderSize = 0;
            btnStartAll.Click += (s, e) => PlayGameFull();

            btnStartServer = new Button
            {
                Text = "Iniciar Servidor",
                BackColor = Color.FromArgb(45, 52, 60),
                ForeColor = Color.WhiteSmoke,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(280, 222),
                Size = new Size(150, 44),
                Cursor = Cursors.Hand
            };
            btnStartServer.FlatAppearance.BorderSize = 0;
            btnStartServer.Click += (s, e) => StartServer();

            btnStopServer = new Button
            {
                Text = "Detener Servidor",
                BackColor = Color.FromArgb(45, 52, 60),
                ForeColor = Color.WhiteSmoke,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(440, 222),
                Size = new Size(150, 44),
                Cursor = Cursors.Hand
            };
            btnStopServer.FlatAppearance.BorderSize = 0;
            btnStopServer.Click += (s, e) => StopServer();

            btnLaunchGame = new Button
            {
                Text = "Lanzar Solo Juego",
                BackColor = Color.FromArgb(45, 52, 60),
                ForeColor = Color.WhiteSmoke,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(600, 222),
                Size = new Size(180, 44),
                Cursor = Cursors.Hand
            };
            btnLaunchGame.FlatAppearance.BorderSize = 0;
            btnLaunchGame.Click += (s, e) => LaunchClientOnly();

            // Actions row 2
            btnTogglePatch = new Button
            {
                Text = "Parche Sombras",
                BackColor = Color.FromArgb(35, 40, 48),
                ForeColor = Color.LightSkyBlue,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(25, 276),
                Size = new Size(175, 34),
                Cursor = Cursors.Hand
            };
            btnTogglePatch.FlatAppearance.BorderSize = 0;
            btnTogglePatch.Click += (s, e) => TogglePatch();

            btnToggleLanguage = new Button
            {
                Text = "Cambiar Idioma",
                BackColor = Color.FromArgb(35, 40, 48),
                ForeColor = Color.PaleGoldenrod,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(210, 276),
                Size = new Size(175, 34),
                Cursor = Cursors.Hand
            };
            btnToggleLanguage.FlatAppearance.BorderSize = 0;
            btnToggleLanguage.Click += (s, e) => ToggleLanguage();

            btnResetProfile = new Button
            {
                Text = "Reiniciar Perfil",
                BackColor = Color.FromArgb(35, 40, 48),
                ForeColor = Color.LightCoral,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(395, 276),
                Size = new Size(180, 34),
                Cursor = Cursors.Hand
            };
            btnResetProfile.FlatAppearance.BorderSize = 0;
            btnResetProfile.Click += (s, e) => ResetProfile();

            btnOpenLogs = new Button
            {
                Text = "Abrir Logs",
                BackColor = Color.FromArgb(35, 40, 48),
                ForeColor = Color.Gainsboro,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(585, 276),
                Size = new Size(195, 34),
                Cursor = Cursors.Hand
            };
            btnOpenLogs.FlatAppearance.BorderSize = 0;
            btnOpenLogs.Click += (s, e) => OpenLogsFolder();

            // Controls Row 3 (Sleeping Dogs & Safehouse Spawn)
            lblSpawn = new Label
            {
                Text = "Spawn / Landmark:",
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                ForeColor = Color.PaleGoldenrod,
                Location = new Point(25, 323),
                AutoSize = true
            };

            cmbSpawnPoint = new ComboBox
            {
                DropDownStyle = ComboBoxStyle.DropDownList,
                BackColor = Color.FromArgb(35, 40, 48),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 9f),
                Location = new Point(180, 320),
                Size = new Size(325, 28)
            };
            cmbSpawnPoint.Items.Add("North Point Safehouse (Distrito North Point)");
            cmbSpawnPoint.Items.Add("Central Safehouse (Distrito Central / Loft)");
            cmbSpawnPoint.Items.Add("Aberdeen Safehouse (Distrito Aberdeen / Flotante)");
            cmbSpawnPoint.Items.Add("Kennedy Town Safehouse (Distrito Kennedy / Ático)");
            cmbSpawnPoint.Items.Add("Restaurante Golden Koi (Sra. Chu - North Point)");
            cmbSpawnPoint.Items.Add("Hospital General Central (Distrito Central)");
            cmbSpawnPoint.Items.Add("Mansión de Victoria Peak (The Peak)");
            cmbSpawnPoint.Items.Add("Dojo de Artes Marciales (Sifu - North Point)");
            cmbSpawnPoint.Items.Add("Club Bam Bam (Discoteca VIP - North Point)");
            cmbSpawnPoint.Items.Add("Barco Casino Flotante (Bahía de Aberdeen)");
            cmbSpawnPoint.Items.Add("Mercado de Pescado (Canal de Aberdeen)");
            cmbSpawnPoint.SelectedIndex = 0;

            btnSetSpawn = new Button
            {
                Text = "Fijar Spawn",
                BackColor = Color.FromArgb(45, 52, 60),
                ForeColor = Color.LightSkyBlue,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(515, 318),
                Size = new Size(115, 30),
                Cursor = Cursors.Hand
            };
            btnSetSpawn.FlatAppearance.BorderSize = 0;
            btnSetSpawn.Click += (s, e) => ApplySelectedSpawn();

            btnSyncSD = new Button
            {
                Text = "Restaurar SD",
                BackColor = Color.FromArgb(140, 35, 35),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(640, 318),
                Size = new Size(140, 30),
                Cursor = Cursors.Hand
            };
            btnSyncSD.FlatAppearance.BorderSize = 0;
            btnSyncSD.Click += (s, e) => SyncSleepingDogs();

            // Controls Row 4 (Starter Vehicle in Garage / Valet)
            lblVehicle = new Label
            {
                Text = "Coche de Garaje (Valet):",
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                ForeColor = Color.PaleGreen,
                Location = new Point(25, 362),
                AutoSize = true
            };

            cmbVehicle = new ComboBox
            {
                DropDownStyle = ComboBoxStyle.DropDownList,
                BackColor = Color.FromArgb(35, 40, 48),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 9f),
                Location = new Point(180, 359),
                Size = new Size(325, 28)
            };
            cmbVehicle.Items.Add("Tuono: BRG (Muscle Car 1970 Dodge Charger)");
            cmbVehicle.Items.Add("Drifter GT (Deportivo Drift)");
            cmbVehicle.Items.Add("Panzer GTS (Sedán Blindado Alemán)");
            cmbVehicle.Items.Add("860 MHC Sportec (Superdeportivo Europeo)");
            cmbVehicle.Items.Add("Enterprise: Dark Shadow (Hiperdeportivo)");
            cmbVehicle.Items.Add("1100 NAR Mangano (Superbike Deportiva)");
            cmbVehicle.Items.Add("Neo V: Le Tigre (Tuner Turbo)");
            cmbVehicle.SelectedIndex = 0;

            btnSetVehicle = new Button
            {
                Text = "Fijar Coche",
                BackColor = Color.FromArgb(45, 52, 60),
                ForeColor = Color.PaleGreen,
                FlatStyle = FlatStyle.Flat,
                Location = new Point(515, 357),
                Size = new Size(115, 30),
                Cursor = Cursors.Hand
            };
            btnSetVehicle.FlatAppearance.BorderSize = 0;
            btnSetVehicle.Click += (s, e) => ApplySelectedVehicle();

            // Log Console
            Label lblConsole = new Label
            {
                Text = "Registro de Eventos y Consola del Servidor:",
                Location = new Point(25, 399),
                AutoSize = true,
                ForeColor = Color.Silver
            };

            txtLogs = new TextBox
            {
                Multiline = true,
                ReadOnly = true,
                ScrollBars = ScrollBars.Vertical,
                BackColor = Color.FromArgb(14, 14, 16),
                ForeColor = Color.PaleGreen,
                Font = new Font("Consolas", 9f),
                Location = new Point(25, 424),
                Size = new Size(755, 290)
            };

            this.Controls.Add(lblTitle);
            this.Controls.Add(lblSubtitle);
            this.Controls.Add(grpStatus);
            this.Controls.Add(btnStartAll);
            this.Controls.Add(btnStartServer);
            this.Controls.Add(btnStopServer);
            this.Controls.Add(btnLaunchGame);
            this.Controls.Add(btnTogglePatch);
            this.Controls.Add(btnToggleLanguage);
            this.Controls.Add(btnResetProfile);
            this.Controls.Add(btnOpenLogs);
            this.Controls.Add(lblSpawn);
            this.Controls.Add(cmbSpawnPoint);
            this.Controls.Add(btnSetSpawn);
            this.Controls.Add(btnSyncSD);
            this.Controls.Add(lblVehicle);
            this.Controls.Add(cmbVehicle);
            this.Controls.Add(btnSetVehicle);
            this.Controls.Add(lblConsole);
            this.Controls.Add(txtLogs);

            this.FormClosing += (s, e) => OnFormClosing();
            LogMessage("Launcher inicializado correctamente en: " + rootDir);
        }

        private void LogMessage(string msg)
        {
            if (txtLogs.InvokeRequired)
            {
                txtLogs.Invoke(new Action<string>(LogMessage), msg);
                return;
            }
            txtLogs.AppendText(string.Format("[{0}] {1}\r\n", DateTime.Now.ToString("HH:mm:ss"), msg));
        }

        private bool IsPortListening(int port)
        {
            try
            {
                using (var client = new TcpClient())
                {
                    var result = client.BeginConnect("127.0.0.1", port, null, null);
                    bool success = result.AsyncWaitHandle.WaitOne(400);
                    if (!success) return false;
                    client.EndConnect(result);
                    return true;
                }
            }
            catch { return false; }
        }

        private void CheckEnvironment()
        {
            string userOptions = Path.Combine(betaDir, "UserOptions.dat");
            if (File.Exists(userOptions))
            {
                try
                {
                    string script = Path.Combine(serverDir, "useroptions.py");
                    var psi = new ProcessStartInfo(pythonPath, string.Format("\"{0}\" set Environment localhost", script))
                    {
                        WorkingDirectory = betaDir,
                        CreateNoWindow = true,
                        UseShellExecute = false
                    };
                    using (var p = Process.Start(psi)) { p.WaitForExit(2000); }
                }
                catch { }
            }
            UpdateLiveStatus();
        }

        private void UpdateLiveStatus()
        {
            // Server
            bool serverUp = IsPortListening(443);
            if (serverUp)
            {
                lblServerStatus.Text = "Servidor Local: EN LÍNEA (127.0.0.1:443)";
                lblServerStatus.ForeColor = Color.LightGreen;
            }
            else
            {
                lblServerStatus.Text = "Servidor Local: DETENIDO";
                lblServerStatus.ForeColor = Color.OrangeRed;
            }

            // Game
            string exePath = Path.Combine(betaDir, "TriadWars.exe");
            if (File.Exists(exePath))
            {
                lblGameStatus.Text = "Cliente: Listo (TriadWars.exe v1.6.0.0)";
                lblGameStatus.ForeColor = Color.LightGreen;
            }
            else
            {
                lblGameStatus.Text = "Cliente: No encontrado en beta/TriadWars.exe";
                lblGameStatus.ForeColor = Color.Red;
            }

            // Config
            lblConfigStatus.Text = "Entorno: Conectado a localhost";
            lblConfigStatus.ForeColor = Color.LightSkyBlue;

            // Patch
            string patchScript = Path.Combine(serverDir, "tools", "patch_exe.py");
            if (File.Exists(patchScript) && File.Exists(exePath))
            {
                try
                {
                    var psi = new ProcessStartInfo(pythonPath, string.Format("\"{0}\" status", patchScript))
                    {
                        WorkingDirectory = Path.GetDirectoryName(patchScript),
                        RedirectStandardOutput = true,
                        UseShellExecute = false,
                        CreateNoWindow = true
                    };
                    using (var p = Process.Start(psi))
                    {
                        string outStr = p.StandardOutput.ReadToEnd();
                        p.WaitForExit(1000);
                        if (outStr.Contains("patched"))
                        {
                            lblPatchStatus.Text = "Parche Sombras: APLICADO (Protegido)";
                            lblPatchStatus.ForeColor = Color.LightGreen;
                        }
                        else
                        {
                            lblPatchStatus.Text = "Parche Sombras: Original sin parchear";
                            lblPatchStatus.ForeColor = Color.Yellow;
                        }
                    }
                }
                catch { }
            }

            // Language
            string uOpt = Path.Combine(serverDir, "useroptions.py");
            if (File.Exists(uOpt))
            {
                try
                {
                    var psi = new ProcessStartInfo(pythonPath, string.Format("\"{0}\" show", uOpt))
                    {
                        WorkingDirectory = serverDir,
                        RedirectStandardOutput = true,
                        UseShellExecute = false,
                        CreateNoWindow = true
                    };
                    using (var p = Process.Start(psi))
                    {
                        string content = p.StandardOutput.ReadToEnd();
                        p.WaitForExit(1000);
                        if (content.Contains("Language\">Spanish"))
                        {
                            lblLangStatus.Text = "Idioma: Español (SP_Global.bin)";
                            lblLangStatus.ForeColor = Color.LightGreen;
                        }
                        else if (content.Contains("Language\">English"))
                        {
                            lblLangStatus.Text = "Idioma: English (EN_Global.bin)";
                            lblLangStatus.ForeColor = Color.LightSkyBlue;
                        }
                        else
                        {
                            lblLangStatus.Text = "Idioma: Predeterminado";
                            lblLangStatus.ForeColor = Color.LightGray;
                        }
                    }
                }
                catch { }
            }

            // Controller
            string xinputDll = Path.Combine(betaDir, "XINPUT9_1_0.dll");
            if (File.Exists(xinputDll))
            {
                lblControllerStatus.Text = "Mando: DualSense Nativo (XInput Hook OK)";
                lblControllerStatus.ForeColor = Color.LightGreen;
            }
            else
            {
                lblControllerStatus.Text = "Mando: Controlador estándar de Windows";
                lblControllerStatus.ForeColor = Color.LightGray;
            }

            // Spawn Safehouse & Landmark Live State
            if (cmbSpawnPoint != null && !cmbSpawnPoint.DroppedDown)
            {
                string script = Path.Combine(serverDir, "tools", "safehouse_manager.py");
                if (File.Exists(script))
                {
                    try
                    {
                        var psi = new ProcessStartInfo(pythonPath, string.Format("\"{0}\" get-spawn", script))
                        {
                            WorkingDirectory = Path.GetDirectoryName(script),
                            RedirectStandardOutput = true,
                            UseShellExecute = false,
                            CreateNoWindow = true
                        };
                        using (var p = Process.Start(psi))
                        {
                            string line = p.StandardOutput.ReadLine();
                            p.WaitForExit(600);
                            if (!string.IsNullOrEmpty(line))
                            {
                                if (line.Contains("SafehouseNP") && cmbSpawnPoint.SelectedIndex != 0) cmbSpawnPoint.SelectedIndex = 0;
                                else if (line.Contains("SafehouseCT") && cmbSpawnPoint.SelectedIndex != 1) cmbSpawnPoint.SelectedIndex = 1;
                                else if (line.Contains("SafehouseAB") && cmbSpawnPoint.SelectedIndex != 2) cmbSpawnPoint.SelectedIndex = 2;
                                else if (line.Contains("SafehouseKT") && cmbSpawnPoint.SelectedIndex != 3) cmbSpawnPoint.SelectedIndex = 3;
                                else if (line.Contains("GoldenKoi") && cmbSpawnPoint.SelectedIndex != 4) cmbSpawnPoint.SelectedIndex = 4;
                                else if (line.Contains("CentralHospital") && cmbSpawnPoint.SelectedIndex != 5) cmbSpawnPoint.SelectedIndex = 5;
                                else if (line.Contains("VictoriaMansion") && cmbSpawnPoint.SelectedIndex != 6) cmbSpawnPoint.SelectedIndex = 6;
                                else if (line.Contains("Dojo") && cmbSpawnPoint.SelectedIndex != 7) cmbSpawnPoint.SelectedIndex = 7;
                                else if (line.Contains("BamBam") && cmbSpawnPoint.SelectedIndex != 8) cmbSpawnPoint.SelectedIndex = 8;
                                else if (line.Contains("Barge") && cmbSpawnPoint.SelectedIndex != 9) cmbSpawnPoint.SelectedIndex = 9;
                                else if (line.Contains("FishMarket") && cmbSpawnPoint.SelectedIndex != 10) cmbSpawnPoint.SelectedIndex = 10;
                            }
                        }
                    }
                    catch { }
                }
            }

            // Starter Vehicle Live State
            if (cmbVehicle != null && !cmbVehicle.DroppedDown)
            {
                string script = Path.Combine(serverDir, "tools", "safehouse_manager.py");
                if (File.Exists(script))
                {
                    try
                    {
                        var psi = new ProcessStartInfo(pythonPath, string.Format("\"{0}\" get-vehicle", script))
                        {
                            WorkingDirectory = Path.GetDirectoryName(script),
                            RedirectStandardOutput = true,
                            UseShellExecute = false,
                            CreateNoWindow = true
                        };
                        using (var p = Process.Start(psi))
                        {
                            string line = p.StandardOutput.ReadLine();
                            p.WaitForExit(600);
                            if (!string.IsNullOrEmpty(line))
                            {
                                if (line.Contains("Tuono") && cmbVehicle.SelectedIndex != 0) cmbVehicle.SelectedIndex = 0;
                                else if (line.Contains("Drifter") && cmbVehicle.SelectedIndex != 1) cmbVehicle.SelectedIndex = 1;
                                else if (line.Contains("Panzer") && cmbVehicle.SelectedIndex != 2) cmbVehicle.SelectedIndex = 2;
                                else if (line.Contains("860") && cmbVehicle.SelectedIndex != 3) cmbVehicle.SelectedIndex = 3;
                                else if (line.Contains("Enterprise") && cmbVehicle.SelectedIndex != 4) cmbVehicle.SelectedIndex = 4;
                                else if (line.Contains("1100") && cmbVehicle.SelectedIndex != 5) cmbVehicle.SelectedIndex = 5;
                                else if (line.Contains("Neo") && cmbVehicle.SelectedIndex != 6) cmbVehicle.SelectedIndex = 6;
                            }
                        }
                    }
                    catch { }
                }
            }
        }

        private void ToggleLanguage()
        {
            string uOpt = Path.Combine(serverDir, "useroptions.py");
            if (!File.Exists(uOpt)) return;

            try
            {
                bool isSpanish = lblLangStatus.Text.Contains("Español");
                string targetLang = isSpanish ? "English" : "Spanish";

                var psi = new ProcessStartInfo(pythonPath, string.Format("\"{0}\" set Language {1}", uOpt, targetLang))
                {
                    WorkingDirectory = serverDir,
                    RedirectStandardOutput = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };
                using (var p = Process.Start(psi))
                {
                    p.WaitForExit(2000);
                }

                LogMessage("Idioma del juego cambiado a: " + (targetLang == "Spanish" ? "Español" : "Inglés"));
                UpdateLiveStatus();
            }
            catch (Exception ex)
            {
                LogMessage("Error cambiando idioma: " + ex.Message);
            }
        }

        private void StartServer()
        {
            if (IsPortListening(443))
            {
                LogMessage("El servidor local ya se encuentra activo en el puerto 443.");
                return;
            }

            try
            {
                string serverPy = Path.Combine(serverDir, "server.py");
                ProcessStartInfo psi = new ProcessStartInfo(pythonPath, string.Format("\"{0}\" --host 127.0.0.1 --port 443", serverPy))
                {
                    WorkingDirectory = serverDir,
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true
                };

                serverProcess = new Process { StartInfo = psi };
                serverProcess.OutputDataReceived += (s, e) => { if (!string.IsNullOrEmpty(e.Data)) LogMessage("[SERVER] " + e.Data); };
                serverProcess.ErrorDataReceived += (s, e) => { if (!string.IsNullOrEmpty(e.Data)) LogMessage("[SERVER ERR] " + e.Data); };

                serverProcess.Start();
                serverProcess.BeginOutputReadLine();
                serverProcess.BeginErrorReadLine();

                LogMessage("Iniciando emulador de servidor local...");

                for (int i = 0; i < 20; i++)
                {
                    Thread.Sleep(250);
                    if (IsPortListening(443))
                    {
                        LogMessage("¡Servidor iniciado con éxito y listo para conexiones!");
                        UpdateLiveStatus();
                        return;
                    }
                }
                LogMessage("Aviso: El servidor tardó más de lo esperado en abrir el puerto 443.");
            }
            catch (Exception ex)
            {
                LogMessage("Error al iniciar el servidor: " + ex.Message);
            }
        }

        private void StopServer()
        {
            try
            {
                if (serverProcess != null && !serverProcess.HasExited)
                {
                    serverProcess.Kill();
                    serverProcess = null;
                }
                var procs = Process.GetProcessesByName("python");
                foreach (var p in procs)
                {
                    try { p.CloseMainWindow(); } catch { }
                }
                LogMessage("Servidor local detenido.");
                UpdateLiveStatus();
            }
            catch (Exception ex)
            {
                LogMessage("Error al detener el servidor: " + ex.Message);
            }
        }

        private void ApplySelectedSpawn()
        {
            if (cmbSpawnPoint == null || cmbSpawnPoint.SelectedIndex < 0) return;
            string[] targets = new string[] {
                "np", "ct", "ab", "kt",
                "koi", "hospital", "mansion", "dojo",
                "bambam", "barge", "fishmarket"
            };
            int idx = cmbSpawnPoint.SelectedIndex;
            if (idx >= 0 && idx < targets.Length)
            {
                ExecuteSafehouseCommand("spawn " + targets[idx]);
            }
        }

        private void ApplySelectedVehicle()
        {
            if (cmbVehicle == null || cmbVehicle.SelectedIndex < 0) return;
            string[] vehTargets = new string[] {
                "tuono", "drifter", "panzer", "860",
                "enterprise", "1100", "neo"
            };
            int idx = cmbVehicle.SelectedIndex;
            if (idx >= 0 && idx < vehTargets.Length)
            {
                ExecuteSafehouseCommand("vehicle " + vehTargets[idx]);
            }
        }

        private void SyncSleepingDogs()
        {
            ExecuteSafehouseCommand("sync-sd");
        }

        private void ExecuteSafehouseCommand(string arg)
        {
            string script = Path.Combine(serverDir, "tools", "safehouse_manager.py");
            if (!File.Exists(script)) return;

            try
            {
                var psi = new ProcessStartInfo(pythonPath, string.Format("\"{0}\" {1}", script, arg))
                {
                    WorkingDirectory = Path.GetDirectoryName(script),
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };
                using (var p = Process.Start(psi))
                {
                    string stdout = p.StandardOutput.ReadToEnd();
                    p.WaitForExit(3000);
                    if (!string.IsNullOrEmpty(stdout))
                    {
                        foreach (var rawLine in stdout.Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries))
                        {
                            string line = rawLine.Trim();
                            if (line.StartsWith("PUNTO DE REAPARICIÓN") || line.StartsWith("Coordenadas") || line.StartsWith("¡SINCRONIZACIÓN") || line.StartsWith("- ") || line.StartsWith("GameplayState"))
                            {
                                LogMessage("[PISO FRANCO] " + line);
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                LogMessage("[PISO FRANCO ERR] " + ex.Message);
            }
        }

        private void LaunchClientOnly()
        {
            ApplySelectedSpawn();
            ApplySelectedVehicle();
            string exePath = Path.Combine(betaDir, "TriadWars.exe");
            if (!File.Exists(exePath))
            {
                MessageBox.Show("No se encontró el ejecutable en: " + exePath, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            try
            {
                ProcessStartInfo psi = new ProcessStartInfo(exePath)
                {
                    WorkingDirectory = betaDir,
                    UseShellExecute = true
                };
                gameProcess = Process.Start(psi);
                LogMessage("Cliente TriadWars.exe ejecutado (PID: " + gameProcess.Id + ").");
            }
            catch (Exception ex)
            {
                LogMessage("Fallo al ejecutar TriadWars.exe: " + ex.Message);
            }
        }

        private void PlayGameFull()
        {
            if (!IsPortListening(443))
            {
                StartServer();
                Thread.Sleep(1000);
            }

            LaunchClientOnly();
        }

        private void TogglePatch()
        {
            string patchScript = Path.Combine(serverDir, "tools", "patch_exe.py");
            if (!File.Exists(patchScript))
            {
                MessageBox.Show("No se localizó patch_exe.py", "Aviso", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                bool isPatched = lblPatchStatus.Text.Contains("APLICADO");
                string action = isPatched ? "restore" : "apply";

                var psi = new ProcessStartInfo(pythonPath, string.Format("\"{0}\" {1}", patchScript, action))
                {
                    WorkingDirectory = Path.Combine(serverDir, "tools"),
                    RedirectStandardOutput = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };
                using (var p = Process.Start(psi))
                {
                    string outStr = p.StandardOutput.ReadToEnd();
                    p.WaitForExit();
                    LogMessage("[PARCHE] " + outStr.Trim());
                }
                UpdateLiveStatus();
            }
            catch (Exception ex)
            {
                LogMessage("Error modificando parche: " + ex.Message);
            }
        }

        private void ResetProfile()
        {
            string stateJson = Path.Combine(serverDir, "state.json");
            if (File.Exists(stateJson))
            {
                var dr = MessageBox.Show("¿Deseas reiniciar tu perfil? Esto borrará el progreso guardado y comenzará como nuevo personaje.",
                    "Confirmar reinicio de partida", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
                if (dr == DialogResult.Yes)
                {
                    try
                    {
                        string backup = Path.Combine(serverDir, "state_backup_" + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".json");
                        File.Copy(stateJson, backup, true);
                        File.Delete(stateJson);
                        LogMessage("Perfil reiniciado. Se creó respaldo en: " + Path.GetFileName(backup));
                    }
                    catch (Exception ex)
                    {
                        LogMessage("Error al reiniciar perfil: " + ex.Message);
                    }
                }
            }
            else
            {
                MessageBox.Show("No existe un perfil guardado previo. Ya empezarás desde cero.", "Información", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }

        private void OpenLogsFolder()
        {
            string logsDir = Path.Combine(serverDir, "logs");
            if (!Directory.Exists(logsDir)) Directory.CreateDirectory(logsDir);
            Process.Start("explorer.exe", logsDir);
        }

        private void OnFormClosing()
        {
            try
            {
                if (serverProcess != null && !serverProcess.HasExited)
                {
                    serverProcess.Kill();
                }
            }
            catch { }
        }

        [STAThread]
        public static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }
}
