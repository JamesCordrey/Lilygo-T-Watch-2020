// ProjectThing.ino

#include "config.h"

// Initialise bluetooth, accelerometer sensor, watch
BluetoothSerial SerialBT;
BMA *sensor;
TTGOClass *ttgo;

// Screen Dimensions
const int screenWidth = 240;
const int screenHeight = 240;

// Snake Game Dimensions
const int topMargin = 20;
const int gridWidth = screenWidth;
const int gridHeight = screenHeight - topMargin;
const int snakeSize = 10;
int snakeX[100] = {0}, snakeY[100] = {0}; // Snake segments
int snakeLength = 5; // Initial Length
int foodX, foodY;
int dirX = 1, dirY = 0; // Initial direction: moving right
bool gameOver = false;
int score = 0;
int highScore = 0;
String lastDirection = "";

// Bool to keep track of whether the bluetooth is enabled
bool bluetoothEnabled = false;

// Date/time variables
uint32_t targetTime = 0;
uint8_t hh, mm, ss, mmonth, dday; // Hour, Minute, Seconds, Months, Days
uint16_t yyear; // Year is 16 bit int

// How many apps we have available in the menu
const int maxApp = 3;
String appName[maxApp] = {"Set Time", "Snake", "BlueTooth"};

void setup() {
    // Initialise watch
    ttgo = TTGOClass::getWatch();
    ttgo->begin();
    ttgo->openBL();
    
    ttgo->tft->fillScreen(TFT_BLACK);

    // Sync system with the clock
    ttgo->rtc->check();
    ttgo->rtc->syncToSystem();

    // Initialize vibration motor
    ttgo->motor_begin();

    // Set up accelerometer
    Acfg cfg;
    cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
    cfg.range = BMA4_ACCEL_RANGE_2G;
    cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;
    cfg.perf_mode = BMA4_CONTINUOUS_MODE;
    sensor = ttgo->bma;
    sensor->accelConfig(cfg);
    sensor->enableAccel();


    // Initialize snake starting position
    for (int i = 0; i < snakeLength; i++) {
        snakeX[i] = (gridWidth / 2) - (i * snakeSize);
        snakeY[i] = gridHeight / 2 + topMargin;
    }
    generateFood();

    // Systems starts with the clock display
    displayTime(true); 
}

// Loop for the snake game
void doGame() {
    if (bluetoothEnabled) {
        SerialBT.print("Snake game started!");
    }

    resetGame();
    while (!gameOver) {
        // Get accelerometer data
        Accel acc;
        bool res = sensor->getAccel(acc);

        // Process accelerometer data into directions
        String direction;
        if (acc.x > 500) {
            direction = "UP";
        } else if (acc.x < -500) {
            direction = "DOWN";
        } else if (acc.y > 500) {
            direction = "RIGHT";
        } else if (acc.y < -500) {
            direction = "LEFT";
        } else {
            direction = "CENTER";
        }

        // Update the game
        if (targetTime < millis()) {
            targetTime = millis() + 100;
            updateGame();
            drawGame();
        }

        // Update the movement direction
        updateDirection(direction);
    }   
    std::string scoreStr = std::to_string(score);
    if (bluetoothEnabled) {
        SerialBT.println(("Snake game finished with score: " + scoreStr).c_str());
    }

    // Update high score
    if (score > highScore) {
        highScore = score;
        if (bluetoothEnabled) {
            SerialBT.println("That's a new highscore!");
        }
    }

    // Vibrate the watch on game over
    ttgo->motor->onec(150);

    // Show the game over UI
    displayGameOverMenu();
}

// Page for the bluetooth enable/disable app
void doBlueTooth() {
    // Print title
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->setTextColor(TFT_WHITE, TFT_BLACK);
    ttgo->tft->setTextSize(3);
    ttgo->tft->setCursor(50, 40);
    ttgo->tft->println("Bluetooth");

    // Create the slider
    drawBluetoothSlider();

    // Create the back button
    ttgo->tft->fillRect(80, 180, 80, 40, TFT_DARKCYAN); // Position and size for back button
    ttgo->tft->setTextColor(TFT_WHITE);
    ttgo->tft->setTextSize(2);
    ttgo->tft->setCursor(100, 190);
    ttgo->tft->println("Back");

    // Listen for screen interactions
    while (true) {
        int16_t x, y;
        if (ttgo->getTouch(x, y)) {
            while (ttgo->getTouch(x, y)) {}
            if (x > 60 && x < 180 && y > 120 && y < 180) { 
                // Begin/end bluetooth connections when slider is tapped
                bluetoothEnabled = !bluetoothEnabled;
                if (bluetoothEnabled) {
                    SerialBT.begin("T-Watch-2020");
                } else {
                    SerialBT.end();
                }
                drawBluetoothSlider(); // Redraw the slider
            } else if (x > 80 && x < 160 && y > 180 && y < 220) { 
                // If back button is pressed, break out of bluetooth app loop
                ttgo->tft->fillScreen(TFT_BLACK);
                delay(100);
                break;
            }
        }
    }
}

// Draws the slider based on whether bluetooth is enabled
void drawBluetoothSlider() {
    if (bluetoothEnabled) {
        ttgo->tft->fillRect(120, 110, 60, 60, TFT_BLUE);
        ttgo->tft->fillRect(60, 110, 60, 60, TFT_LIGHTGREY);
    } else {
        ttgo->tft->fillRect(60, 110, 60, 60, TFT_RED);
        ttgo->tft->fillRect(120, 110, 60, 60, TFT_LIGHTGREY);
    }
}

// Main project loop
void loop() {
    // Start of loop, display time page
    if (targetTime < millis()) {
        targetTime = millis() + 1000;
        displayTime(ss == 0);
    }

    // When screen is touched, go to the app menu
    int16_t x, y;
    if (ttgo->getTouch(x, y)) {
        while (ttgo->getTouch(x, y)) {}
        switch (modeMenu()) {
            case 0: appSetTime(); break;
            case 1: // Snake game
                doGame();
                break;
            case 2:
                doBlueTooth();
                break;
                
        }
        displayTime(true);
    }
}

// Function to display the time page
void displayTime(boolean fullUpdate) {
    const int xpos = 40; // Starting position for the display
    const int ypos = 80; // Adjusted position for better display

    // Get date
    RTC_Date tnow = ttgo->rtc->getDateTime();
    hh = tnow.hour;
    mm = tnow.minute;
    ss = tnow.second;
    dday = tnow.day;
    mmonth = tnow.month;
    yyear = tnow.year;

    ttgo->tft->setTextSize(2);

    if (fullUpdate) {
        // Clear the previous time display
        ttgo->tft->fillRect(xpos, ypos, 200, 50, TFT_BLACK);
    }

    // Update the time display every second
    ttgo->tft->setTextColor(TFT_WHITE, TFT_BLACK);
    ttgo->tft->setTextSize(7);
    ttgo->tft->drawNumber(hh / 10, xpos, ypos);
    ttgo->tft->drawNumber(hh % 10, xpos + 40, ypos);
    ttgo->tft->drawNumber(mm / 10, xpos + 100, ypos);
    ttgo->tft->drawNumber(mm % 10, xpos + 135, ypos);

    // Blink the colon
    ttgo->tft->setTextColor((ss % 2) ? TFT_BLACK : TFT_WHITE, TFT_BLACK);
    ttgo->tft->setTextSize(2);
    ttgo->tft->drawString(":", xpos + 80, ypos, 4);

    // Draw the date at the bottom
    ttgo->tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    ttgo->tft->setTextSize(3);
    ttgo->tft->setCursor(10, 200);
    ttgo->tft->print(mmonth);
    ttgo->tft->print("/");
    ttgo->tft->print(dday);
    ttgo->tft->print("/");
    ttgo->tft->print(yyear);
}

// Display the menu
uint8_t modeMenu() {
    int mSelect = 0;
    int16_t x, y;
    boolean exitMenu = false;
    setMenuDisplay(0);
    // If center app in menu is clicked, open app. Otherwise, clicked app is moved to center.
    while (!exitMenu) {
        if (ttgo->getTouch(x, y)) {
            while (ttgo->getTouch(x, y)) {}
            if (y >= 160) { mSelect += 1; if (mSelect == maxApp) mSelect = 0; setMenuDisplay(mSelect); }
            if (y <= 80) { mSelect -= 1; if (mSelect < 0) mSelect = maxApp - 1; setMenuDisplay(mSelect); }
            if (y > 80 && y < 160) { exitMenu = true; }
        }
    }
    ttgo->tft->fillScreen(TFT_BLACK);
    return mSelect;
}

// Function to draw the menu
void setMenuDisplay(int mSel) {
    int curSel = 0;
    ttgo->tft->fillScreen(TFT_BLUE);
    ttgo->tft->fillRect(0, 80, 239, 80, TFT_BLACK);
    if (mSel == 0) curSel = maxApp - 1;
    else curSel = mSel - 1;
    ttgo->tft->setTextSize(2);
    ttgo->tft->setTextColor(TFT_GREEN);
    ttgo->tft->setCursor(50, 30);
    ttgo->tft->println(appName[curSel]);
    ttgo->tft->setTextSize(3);
    ttgo->tft->setTextColor(TFT_RED);
    ttgo->tft->setCursor(40, 110);
    ttgo->tft->println(appName[mSel]);
    if (mSel == maxApp - 1) curSel = 0;
    else curSel = mSel + 1;
    ttgo->tft->setTextSize(2);
    ttgo->tft->setTextColor(TFT_GREEN);
    ttgo->tft->setCursor(50, 190);
    ttgo->tft->print(appName[curSel]);
}

void updateGame() {
    // Move all segements of the snake
    for (int i = snakeLength - 1; i > 0; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
    }
    snakeX[0] += dirX * snakeSize;
    snakeY[0] += dirY * snakeSize;

    if (snakeX[0] < 0 || snakeX[0] >= gridWidth || snakeY[0] < topMargin || snakeY[0] >= screenHeight) {
        gameOver = true;
    }
    for (int i = 1; i < snakeLength; i++) {
        if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
            gameOver = true;
        }
    }
    // If snake collides with apple, increase length, score and spawn new apple
    if (snakeX[0] < foodX + snakeSize * 2 && snakeX[0] + snakeSize > foodX &&
        snakeY[0] < foodY + snakeSize * 2 && snakeY[0] + snakeSize > foodY) {
        snakeLength++;
        score++;
        if (bluetoothEnabled) {
            std::string scoreStr = std::to_string(score);
            SerialBT.println(("Snake has eaten an apple, new score: " + scoreStr).c_str());
        }
        generateFood(); // New apple
    }
}

// Redraw the game to update score, snake position, apple etc.
void drawGame() {
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->setTextColor(TFT_WHITE);
    ttgo->tft->setTextSize(2);
    ttgo->tft->drawString("Score: " + String(score), 10, 0, 2);
    ttgo->tft->drawString("High Score: " + String(highScore), 10, 20, 2);
    for (int i = 0; i < snakeLength; i++) {
        ttgo->tft->fillRect(snakeX[i], snakeY[i], snakeSize, snakeSize, TFT_GREEN);
    }
    ttgo->tft->fillRect(foodX, foodY, snakeSize * 2, snakeSize * 2, TFT_RED); 
}

// Draws the game over UI
void displayGameOverMenu() {
    // Game over message
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->setTextColor(TFT_WHITE, TFT_BLACK);
    ttgo->tft->setTextSize(3);
    ttgo->tft->setCursor(50, 80);
    ttgo->tft->println("Game Over");

    // Draw Restart button
    ttgo->tft->setTextColor(TFT_WHITE);
    ttgo->tft->setTextSize(2);
    ttgo->tft->fillRect(20, 150, 90, 40, TFT_BLUE);
    ttgo->tft->setCursor(30, 160);
    ttgo->tft->println("Again");

    // Draw Quit button
    ttgo->tft->fillRect(130, 150, 90, 40, TFT_RED);
    ttgo->tft->setCursor(150, 160);
    ttgo->tft->println("Quit");

    // Listen for screen interaction
    while (true) {
        int16_t x, y;
        if (ttgo->getTouch(x, y)) {
            if (x >= 20 && x <= 110 && y >= 150 && y <= 190) {
                ttgo->tft->fillScreen(TFT_BLACK);
                doGame();  // Restart the game
                delay(100);
                break;
            } else if (x >= 130 && x <= 220 && y >= 150 && y <= 190) {
                ttgo->tft->fillScreen(TFT_BLACK);
                displayTime(true);  // Back to home page (time display)
                delay(100);
                break;
            }
        }
    }
}

// Generate a new apple position
void generateFood() {
    foodX = random(gridWidth / snakeSize - 2) * snakeSize; // Adjusted to prevent spawning outside the grid
    foodY = random(gridHeight / snakeSize - 2) * snakeSize + topMargin; // Adjusted to prevent spawning outside the grid
}

// Restart the game to initial positions
void resetGame() {
    snakeLength = 5;
    dirX = 1; dirY = 0;
    gameOver = false;
    score = 0; // Reset score
    for (int i = 0; i < snakeLength; i++) {
        snakeX[i] = (gridWidth / 2) - (i * snakeSize);
        snakeY[i] = gridHeight / 2 + topMargin;
    }
    generateFood();
}

// Updates the movement direction of the snake based on accelerometer direction
void updateDirection(String direction) {
    if (direction != "CENTER") {
        if (direction == "UP") {
            if (dirY != 1) {
                dirX = 0;
                dirY = -1;
            }
        } else if (direction == "DOWN") {
            if (dirY != -1) {
                dirX = 0;
                dirY = 1;
            }
        } else if (direction == "LEFT") {
            if (dirX != 1) {
                dirX = -1;
                dirY = 0;
            }
        } else if (direction == "RIGHT") {
            if (dirX != -1) {
                dirX = 1;
                dirY = 0;
            }
        }

        if (lastDirection != direction) {
            if (bluetoothEnabled) {
            SerialBT.println("Snake direction changed: " + direction);
            }
            // Vibrate on direction change
            ttgo->motor->onec(50);
        }
        lastDirection = direction;
    }
}

// App for altering the time for the time display
void appSetTime() {
    // Get the current info
    RTC_Date tnow = ttgo->rtc->getDateTime();
    hh = tnow.hour;
    mm = tnow.minute;
    ss = tnow.second;
    dday = tnow.day;
    mmonth = tnow.month;
    yyear = tnow.year;

    // Draw the UI
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->fillRect(20, 50, 80, 40, TFT_BLUE); // Hour Up
    ttgo->tft->fillRect(140, 50, 80, 40, TFT_BLUE); // Hour Down
    ttgo->tft->fillRect(20, 120, 80, 40, TFT_BLUE); // Minute Up
    ttgo->tft->fillRect(140, 120, 80, 40, TFT_BLUE); // Minute Down
    ttgo->tft->fillRect(70, 190, 100, 40, TFT_DARKCYAN); // Done

    // Button labels
    ttgo->tft->setTextColor(TFT_WHITE);
    ttgo->tft->setTextSize(2);
    ttgo->tft->setCursor(35, 60);
    ttgo->tft->print("H+");
    ttgo->tft->setCursor(160, 60);
    ttgo->tft->print("H-");
    ttgo->tft->setCursor(35, 130);
    ttgo->tft->print("M+");
    ttgo->tft->setCursor(160, 130);
    ttgo->tft->print("M-");
    ttgo->tft->setCursor(90, 200);
    ttgo->tft->print("DONE");

    prtTime(0); // Display the time

    // Listen for screen interaction
    while (true) {
        int16_t x, y;
        if (ttgo->getTouch(x, y)) {
            while (ttgo->getTouch(x, y)) {}
            if (x > 20 && x < 100 && y > 50 && y < 90) {
                hh = (hh + 1) % 24;
                prtTime(0);
            } else if (x > 140 && x < 220 && y > 50 && y < 90) {
                hh = (hh + 23) % 24; 
                prtTime(0);
            } else if (x > 20 && x < 100 && y > 120 && y < 160) {
                mm = (mm + 1) % 60;
                prtTime(0);
            } else if (x > 140 && x < 220 && y > 120 && y < 160) {
                mm = (mm + 59) % 60; 
                prtTime(0);
            } else if (x > 70 && x < 170 && y > 190 && y < 230) {
                ttgo->rtc->setDateTime(yyear, mmonth, dday, hh, mm, 0);
                ttgo->tft->fillScreen(TFT_BLACK);
                return;
            }
        }
    }
}

// Display the current selected time
void prtTime(byte digit) {
    ttgo->tft->fillRect(60, 0, 120, 50, TFT_BLACK);
    ttgo->tft->setTextColor(TFT_WHITE);
    ttgo->tft->setTextSize(4);
    ttgo->tft->setCursor(70, 10);
    if (hh < 10) ttgo->tft->print("0");
    ttgo->tft->print(hh);
    ttgo->tft->print(":");
    if (mm < 10) ttgo->tft->print("0");
    ttgo->tft->print(mm);
}
