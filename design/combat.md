# Naval Warfare and Trade System — Age of Discovery

**Purpose:**  
To design a deep but streamlined naval and trade system emphasizing visible fleets, adjacency bonuses, and tactical decision-making—avoiding micromanagement while rewarding smart positioning and timing.

---

## ⚓ 1. Core Design Principles

- **If you can see it, you can save it.**  
  Trade and combat occur directly on the map, not in menus.

- **Strategic decisions, tactical automation.**  
  Player choices create systems that run themselves until disrupted.

- **Momentum & facing define power.**  
  Movement and previous turn actions determine firing readiness.

- **Adjacency is strategy.**  
  Ships and cities influence one another through proximity rather than extra clicks.

---

## 🌊 2. Ship Classes and Roles

| Class | Primary Role | Key Traits |
|--------|---------------|------------|
| **Caravel** | Exploration / Scouting | High speed, extended vision radius. |
| **Galleon** | Merchant / Transport | Carries trade goods, generates income per route. |
| **Frigate** | Escort / Skirmisher | Protects nearby convoys, bonuses vs. Privateers. |
| **Ship of the Line** | Heavy Combat | Broadside specialist, morale aura for fleet. |
| **Privateer** | Raider | Can attack merchant convoys, stealth until adjacent. |
| **Flagship / Admiral** | Command Unit | Provides morale + accuracy aura; doctrine bonuses. |

---

## ⚔️ 3. Broadside Combat System

**Concept:**  
Each ship alternates between *positioning* and *firing* turns.

### Turn Flow
1. **If ship moved last turn:** Cannons are reloading → cannot fire.  
2. **If ship held position last turn:** Cannons are loaded → can fire one side (port/starboard).  
3. **Fire Action:** Broadside damage calculated by facing vs. target’s angle.  
4. **Movement:** Optional—sacrifice next turn’s fire readiness for new positioning.

### Facing & Arcs
- Two 90° arcs: **Port** and **Starboard.**
- Damage multipliers:
  - 1.0× front / rear
  - 1.3× side (broadside)
  - 1.5× “Crossing the T” (enemy bow/stern in arc)

### Wind System
- Global wind vector changes periodically.
- +1 move with wind, −1 against.
- Slight accuracy bonus when firing with wind.

### Adjacency & Fleet Synergy
| Formation | Effect |
|------------|---------|
| **Line of Battle** | Adjacent Ships of the Line gain +15% accuracy, −10% damage taken. |
| **Convoy (Galleon + Frigate)** | +Trade yield, reduced raid risk. |
| **Wolf Pack (Privateers)** | Chance to ambush convoys in range. |
| **Scout Screen (Caravels ahead)** | +Vision for fleet cluster. |
| **Admiral’s Aura** | Amplifies adjacency effects by +25%. |

---

## 💰 4. Trade and Convoy System

**Goal:** Create trade routes that can be manually protected and visually contested.  

### Route Mechanics
- Player designates **Port A → Port B** trade route.  
- A **Merchant Convoy** spawns and travels the lane each turn.  
- Upon arrival: delivers resources and gold.  
- Longer routes = higher reward, higher risk.

### Escort Mechanics
- Any **escort ship** within 1–2 hexes provides protection.  
- Multiple escorts stack diminishing returns (coverage & morale).  
- Player must manually maneuver escorts to stay near convoys.  
- If no escort nearby → convoy vulnerable to raid.

### Raid Resolution
- Each ocean tile has a **Piracy Risk** rating.  
- If enemy privateer or hostile fleet enters proximity:
  - `if random() < risk - escort_strength`: trigger raid.  
  - Result: cargo loss, delay, or ship capture.  
  - Adjacent escorts may intercept and trigger mini-skirmish.

### Visual Feedback
- Trade lines glow: Green (safe) → Yellow (threat) → Red (danger).  
- Tooltip example:  
  > “Convoy to Lisbon — Safety 72% (Frigate escorting). Next delivery: 2 turns.”

---

## 🧭 5. Exploration & Discovery

- Caravels can **Chart Waters**: mark sea lanes that grant +movement for friendly fleets.  
- Returning to port after exploration yields **prestige** and **map data** for empire bonuses.  
- Naval Academies and Universities can train Admirals with special doctrines (Navigation, Artillery, Logistics).

---

## 🏴‍☠️ 6. Pirates & Dynamic Risk

- **Pirate Havens** increase nearby raid risk until destroyed.  
- Risk decays over time if patrolled by military ships.  
- Weather events (storms, fog) can temporarily boost risk.  
- Privateers may spawn during wars—semi-autonomous raiders.

---

## 🧱 7. City & Institutional Influence on Admirals

- Admirals stationed at specific city types adopt new **Auras**:  
  - **University** → Siegecraft / Gunnery +range.  
  - **Naval College** → Navigation / Maneuver +movement.  
  - **Holy City** → Morale aura bonus.  
  - **Industrial Port** → Faster ship repair.  
- Staying 3–5 turns in a city applies the aura automatically—no menus required.

---

## ⚙️ 8. Implementation Notes

- Each fleet stores:
  ```yaml
  {
    position: (x, y),
    facing: degrees,
    last_action: "move" | "hold",
    can_fire: bool,
    escort_strength: float,
    convoy_id: optional
  }
