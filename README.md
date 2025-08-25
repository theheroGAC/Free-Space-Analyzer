# Free Space Analyzer (PS Vita)

![PS Vita](https://img.shields.io/badge/Platform-PS%20Vita-blue)  
![License](https://img.shields.io/badge/License-MIT-green)

A homebrew tool for the PlayStation Vita to **analyze free and used space** across system partitions.  
Displays usage bars, top folders, battery/FPS info, and shows a custom splash screen at startup.  

---

## ✨ Features
- Detects and lists all Vita partitions (`ux0`, `ur0`, `uma0`, etc.)  
- Visual usage bar (used vs free space)  
- Shows top folders/files in the selected partition  
- Battery level and FPS indicator  
- Custom splash screen (`reihen.png`)  
- Navigation:  
  - **Stick Up/Down** → Change partition  
  - **X** → Exit  

---

## 📥 Installation
1. Download the latest **VPK** from the [Releases](../../releases) page.  
2. Copy the `.vpk` to your Vita (USB, FTP, or QCMA).  
3. Install it using **[VitaShell](https://github.com/TheOfficialFloW/VitaShell)**.  
4. Launch **Free Space Analyzer** from your LiveArea.  

---

## 🔧 Requirements
- PlayStation Vita or PSTV with **HENkaku/enso** (firmware 3.60/3.65 recommended).  
- **VitaShell** to install `.vpk` packages.  

---

## 🛠️ Build from source
1. Install **[VitaSDK](https://vitasdk.org)**.  
2. Make sure the `VITASDK` environment variable is set.  
3. Clone this repository:  

   ```bash
   git clone https://github.com/yourusername/vita-freespace-analyzer.git
   cd vita-freespace-analyzer
