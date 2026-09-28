#ifndef ZEPHYREBOOK_MYCODE_EBOOK_ENCODERMANAGER_H
#define ZEPHYREBOOK_MYCODE_EBOOK_ENCODERMANAGER_H
#include <Arduino.h>
#include <cstdint>
#include <esp32-hal-gpio.h>
#include <esp_attr.h>

#include "EBook_Config.h"

static const unsigned long DEBOUNCE_MS = 20 ; //消抖时间阈值
static const unsigned long LONG_PRESS_MS = 1000 ; //长按

enum EncoderState {
    ENCODER_NONE ,
    ENCODER_UP ,
    ENCODER_DOWN ,
    ENCODER_OK ,
    ENCODER_BACK ,
};

struct EncoderPressDefine {
    uint8_t Pin ;
    bool CurrentState ;  //消抖后当前稳定状态 true 为按下，false为松开
    bool LastState ;
    bool Pressed ;   //按下边缘时间标志，仅按下那一帧为true ，下同
    bool Released ;
    bool LongPressTriggered ;
    unsigned long PressTime ;   //按下那一瞬间的时间戳
    unsigned long LastDebounceTime ;  //电平最后一次发生变化的时间戳
};

struct EncoderRotateDefine {
    int EncoderLastPositon;
    unsigned long EncoderLastRotateTime;
    volatile int EncoderPosition = 0 ;
};

// ===== 中断处理函数，记录旋转编码器位置 ===== //
volatile int GlobalEncoderPosition = 0 ;
inline void IRAM_ATTR EncoderPosition_ISR() {
    if (digitalRead(ENCODER_PIN_A) == digitalRead(ENCODER_PIN_B))  GlobalEncoderPosition++ ;
    else  GlobalEncoderPosition-- ;
}

class EncoderManager {
private:
    uint8_t PinA = ENCODER_PIN_A ;
    uint8_t PinB = ENCODER_PIN_B ;
    uint8_t PinS = ENCODER_PIN_S ;
    EncoderPressDefine Press ;
    EncoderRotateDefine Rotate ;
    EncoderState EncoderCurrentEvent;

public:
    EncoderManager() {
        Press = {PinS , false , false , false ,false , false,0 , 0} ;
        Rotate.EncoderPosition = 0;
        Rotate.EncoderLastPositon = 0;
        EncoderCurrentEvent = ENCODER_NONE;
    }
    void Init() {
        pinMode(PinA, INPUT_PULLUP) ;
        pinMode(PinB, INPUT_PULLUP) ;
        pinMode(PinS, INPUT_PULLUP) ;
        attachInterrupt(digitalPinToInterrupt(PinA), EncoderPosition_ISR, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PinB), EncoderPosition_ISR, CHANGE);
        Serial.println("[Encoder] Init Successfully !");
    }

    //每一帧刷新一次 , 判断按键状态
    void update() {
        //===== 判断编码器的转动 =====//
        Rotate.EncoderPosition = GlobalEncoderPosition;
        EncoderCurrentEvent = ENCODER_NONE;
        unsigned long now = millis();
        if (Rotate.EncoderPosition != Rotate.EncoderLastPositon) {
            if (now - Rotate.EncoderLastRotateTime > DEBOUNCE_MS ) {
                if (Rotate.EncoderPosition > Rotate.EncoderLastPositon) {
                    EncoderCurrentEvent = ENCODER_DOWN ;  //顺时针转
                }
                else {
                    EncoderCurrentEvent = ENCODER_UP ; //逆时针转
                }
                Rotate.EncoderLastPositon = Rotate.EncoderPosition;
                Rotate.EncoderLastRotateTime = now;
            }
        }
        //===== 判断按键长按或者短按 =====//
        bool ReadPress = (digitalRead(PinS) == LOW)  ;
        if (ReadPress != Press.LastState) {
            Press.LastDebounceTime = now ;
            Press.LastState = ReadPress ;
        }
        if (now - Press.LastDebounceTime > DEBOUNCE_MS) {
            if (ReadPress != Press.CurrentState) {
                Press.CurrentState = ReadPress  ;
                //以上全是消抖
                if (ReadPress) {
                    Press.Pressed = true;
                    Press.PressTime = now;
                    Press.LongPressTriggered = false ;
                }
                else {
                    Press.Released = true;
                    if (!Press.LongPressTriggered) {
                        EncoderCurrentEvent = ENCODER_OK ;
                    }
                }
            }
        }
        if (Press.CurrentState && !Press.LongPressTriggered) {
            if (now - Press.LastDebounceTime > LONG_PRESS_MS) {
                EncoderCurrentEvent = ENCODER_BACK ;
                Press.LongPressTriggered = true ;
            }
        }
    }
    EncoderState GetEvent() {
        return EncoderCurrentEvent ;
    }

    void Reset() {
        EncoderCurrentEvent = ENCODER_NONE ;
        Press.Pressed = false ;
        Press.Released = false ;
        Press.LongPressTriggered = false ;
    }
};

#endif //ZEPHYREBOOK_MYCODE_EBOOK_ENCODERMANAGER_H
