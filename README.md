# 🐝 Bee Flower Guessing Game

## 📌 Overview

The Bee Flower Guessing Game is a small embedded game developed using the **STM32F031K6** and PlatformIO.

In the game, the player controls a bee sprite and tries to guess the correct flower from eight randomly generated options.

The project was created to practice microcontroller hardware interaction, input/output control, and basic embedded game logic.

---

## 🖥 Technical Information

- Microprocessor: STM32F031K6  
- Programming Language: C  
- Display Resolution: 128px × 160px  
- Development Environment: PlatformIO (VSCode)  
- Sprite Editor: [LibreSprite](https://libresprite.github.io/)
---

## 🎮 Game Features

- Main menu interface  
- Sprite-based bee character and flower targets  
- Random flower selection each round  
- Score tracking system  
- Three chances per round  
- Restart / replay functionality  
- LED feedback:
  - Red LED → Incorrect selection  
  - Green LED → Correct selection  
- Buzzer sound output

---

## 🎛 Controls

- ⬆️ UP button → Move up / Start game  
- ⬇️ DOWN button → Move down  
- ⬅️ LEFT button → Move left  
- ➡️ RIGHT button → Move right  
- Q Key → Quit game  
- R Key → Restart after Game Over  

---

<!-- ## ⚙ Implementation Notes

- State-machine based game logic  
  - MENU  
  - PLAYING  
  - GAME OVER  

- UART serial communication is used for debugging and terminal output.

- Timer-based frequency control is used for buzzer sound generation.

---

## 📷 Screenshots

### Main Menu
<p align="center">
  <img src="images/main_menu.png" width="400">
</p>

### Gameplay
<p align="center">
  <img src="images/gameplay.png" width="400">
</p>

### Hardware Setup
<p align="center">
  <img src="images/hardware_setup.png" width="400">
</p>

---

## 🚀 Future Improvements

- Add difficulty progression  
- Store high scores in flash memory  
- Improve sprite animation
- Explore real-time scheduling using [FreeRTOS](https://www.freertos.org/)
---
-->
## ⭐ Conclusion

This project demonstrates basic embedded systems development using PlatformIO and the **STM32F031K6**, combining hardware input/output, simple graphics rendering, and interactive game logic.