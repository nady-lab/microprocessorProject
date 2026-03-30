# 🐝 Buzz Rush

## 📌 Overview

**Buzz Rush** is an embedded systems game developed for the **STM32F031K6** microcontroller using C and PlatformIO.

The player controls a bee that moves around the screen and must identify the **correct flower** from eight randomly generated options. Selecting the correct flower increases the score, while incorrect choices reduce lives.

The project demonstrates core embedded programming concepts including **GPIO input handling, SPI display control, UART communication, timers, interrupts, and sound generation**.

---

## 🖥 Technical Information

- **Microcontroller:** STM32F031K6  
- **Programming Language:** C  
- **Display:** 128 × 160 SPI LCD  
- **Development Environment:** PlatformIO (VS Code)  
- **Graphics:** Custom sprites (LibreSprite)  
- **Communication:** UART (serial input/output)  
- **Audio:** Timer-based sound generation (PWM via Timer14)

---

## 🎮 Game Features

- **Main Menu System**
  - Start screen with instructions
  - Button-controlled navigation

- **Player Movement**
  - Controlled using hardware buttons (GPIO input)
  - Direction-based sprite rendering

- **Randomised Gameplay**
  - Random correct flower each round (`rand() % 8`)
  - Random flower appearances for visual variation

- **Lives & Score System**
  - Score increases when correct flower is selected
  - Lives decrease when incorrect flower is selected
  - Real-time HUD display

- **LED Feedback**
  - Green LED → Correct choice  
  - Red LED → Incorrect choice  

- **Sound Effects**
  - Positive sound sequence for correct selection  
  - Negative sound for incorrect selection  
  - Game over sound effect  
  - Implemented using timer-based frequency control

- **Pause Functionality**
  - Dedicated button pauses and resumes the game
  - Game state is preserved and restored

- **Restart / Replay**
  - Game restarts from Game Over screen
  - Full reset of score, lives, and game state

- **Serial Communication (UART)**
  - Press **Q/q** in terminal to quit the game
  - Debug messages printed (score, lives, events)

---

## 🎛 Controls

| Input | Action |
|------|--------|
| ⬆️ UP | Move up / Start game / Restart |
| ⬇️ DOWN | Move down |
| ⬅️ LEFT | Move left |
| ➡️ RIGHT | Move right |
| 🔘 Button (PB0) | Pause / Resume |
| ⌨️ Q / q | Quit game (via serial terminal) |

---

## ⚙ Implementation Details

- **Game Loop Structure**
  - Outer loop → handles restarting the game  
  - Inner loop → handles active gameplay  

- **Timing System**
  - SysTick interrupt runs every **1ms**
  - Used for delays and game timing control

- **Collision Detection**
  - Based on bounding box checks using `isInside()`
  - Detects overlap between bee and flowers

- **Game Logic**
  - Correct flower stored as an index
  - Collision triggers:
    - Score/life updates
    - Sound + LED feedback
    - Flower reset and repositioning

- **Rendering System**
  - SPI communication used to draw pixels and sprites
  - Screen updated only when movement occurs → reduces flicker

---

## ⭐ Conclusion

This project demonstrates a complete embedded game system using the STM32 platform. It integrates **real-time input handling, graphical rendering, sound generation, and serial communication** into a fully interactive application.

---

## 🚀 Future Improvements

- Add difficulty scaling (faster movement, more flowers)
- Store high scores in non-volatile memory
- Improve animation (frame-based movement)
- Add background music
- Implement RTOS (e.g., FreeRTOS) for task management
