# 🧭 Control Scheme — Direction-Aware Movement & Map Navigation

**Goal:**  
Keep controls minimal, tactile, and readable — every command happens directly on the map with a single click, drag, or scroll.

---

## 🎮 Unit Controls

| Action | Control | Description |
|--------|----------|-------------|
| **Select Unit** | **Left-Click** | Highlights unit’s hex; shows facing arrow and movement range overlay. |
| **Move (Direction-Aware)** | **Left-Click-Drag** to destination tile → **Release** | Draws path ribbon; ghost arrow shows predicted facing (direction of final step). |
| **Confirm Move** | **Release Mouse Button** | Unit travels along path; ends facing direction shown. |
| **Rotate Facing (In-Place)** | **Scroll Wheel** or **Right-Click-Drag around unit** | Rotates unit in 60° increments (hex map) or 45° (square map). Costs small movement/action. |
| **Quick Rotate** | **Q / E** | Snap rotate left/right by one facing increment. |
| **Face Nearest Enemy** | **R** | Automatically orient toward closest visible enemy. |
| **Hold Facing / Brace** | **F** | Locks current facing; prevents auto-rotation next movement. |
| **Queue Movements / Rotations** | **Shift + Click** | Queue multiple path or facing commands. |
| **Attack** | **Right-Click Enemy Tile** | Auto-faces target; displays attack arc preview (front/flank/rear). |
| **Formation Rotate** | **Ctrl + Drag** | Rotate entire selected formation around pivot hex. |
| **Show Vision / Attack Cone** | **Alt (Hold)** | Toggle overlay of unit’s sight and attack arcs. |

---

## 🗺️ Map Navigation

| Action | Control | Description |
|--------|----------|-------------|
| **Pan Map** | **Middle-Mouse Drag** or **W / A / S / D** | Moves camera across the map. |
| **Zoom In / Out** | **Scroll Wheel** | Adjusts camera zoom level. |
| **Reset Camera** | **Double-Click Empty Tile** or **Home Key** | Centers camera on current selection or capital city. |
| **Rotate Map (optional)** | **Hold Alt + Scroll Wheel** | Rotates map view; does not change unit facing. |

---

## 🧩 Visual Feedback

- **Path Preview:** Curved ribbon along selected route; final tile shows ghost facing arrow.  
- **Rotation Indicator:** Circular dial ticks at valid facing increments.  
- **Flank / Rear Arcs:** Colored hex edges when hovering enemies (green = rear, yellow = flank, gray = front).  
- **Idle Units:** Subtle pulsing ring prompting user attention.  

---

### Design Ethos

> *Every control should piggyback on natural mouse movement — no pop-up menus, no mode switches.  
> Strategy happens through motion, not UI.*  
