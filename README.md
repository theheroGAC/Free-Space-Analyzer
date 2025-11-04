# Free Space Analyzer (PS Vita)

![PS Vita](https://img.shields.io/badge/Platform-PS%20Vita-blue)  
![License](https://img.shields.io/badge/License-MIT-green)

A homebrew tool for the PlayStation Vita to **analyze free and used space** across system partitions.  
Displays usage bars, top folders, battery/FPS info, and shows a custom splash screen at startup.  

---

##  Features
- Detects and lists all Vita partitions (`ux0`, `ur0`, `uma0`, etc.)  
- Visual usage bar (used vs free space)  
- Shows top folders/files in the selected partition  
- Battery level and FPS indicator  
- Custom splash screen (`reihen.png`)  
##  Controls

### **Main Navigation**
- **Left Stick Up/Down** → Change partition (ux0, ur0, uma0, etc.)
- **D-Pad Up/Down** → Navigate through folders/files in current partition
- **X Button** → Enter selected folder
- **O Button** → Go back to parent folder
- **Square Button** → Open filter menu (All, Games, MP3, OGG, Photo, Video, Docs, Archives, Homebrew, SaveData)
- **Triangle Button** → Exit application
- **R Trigger** → Delete selected file/folder (with confirmation dialog)

### **Filter Menu (when opened with Square)**
- **D-Pad Up/Down** → Navigate filter options
- **X Button** → Select filter and apply
- **O Button** → Cancel and close menu

### **Delete Confirmation Dialog**
- **X Button** → Confirm deletion (Yes)
- **O Button** → Cancel deletion (No)

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
