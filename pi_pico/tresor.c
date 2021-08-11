#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/multicore.h"

uint totalNumbers = 120;
int totalSteps = 12800;

const uint LED_PIN = 25;
const uint DIR_PIN = 14;
const uint PUL_PIN = 15;

const uint STEP_ENC_EA = 16;
const uint STEP_ENC_EB = 17;

const uint stepTime = 90;
int currentStep = 0;

enum DIR {
    right = false,
    left = true
};
enum DIR currentDirection = right;

enum motorRamp {
    normal = 0,
    start = 1,
    stop = 2,
    startstop = 3
};

int currentNumber = 0;

// encoder
const uint ENC_CLK_PIN = 12;
const uint ENC_DT_PIN = 13;
const uint ENC_SW_PIN = 20;

uint32_t enc_sw_last_pressed = 0;
uint32_t enc_last_rotation = 0;

bool doCorrection = true;

// hall sensor
const uint HALL_SIGNAL_PIN = 8;
const uint HALL_PWR_PIN = 9;

volatile uint32_t spikes = 0;

volatile int encoder_step = 0;
volatile bool last_ea = true;
volatile bool last_eb = true;

void doEncoderA(int value_a){
    int value_b = gpio_get(STEP_ENC_EB);
    if(value_a){
        if(value_b == 0){
            encoder_step++;
        } else {
            encoder_step--;
        }
    } else {
        if(value_b){
            encoder_step++;
        } else {
            encoder_step--;
        }

    }
}

void doEncoderB(int value_b){
    int value_a = gpio_get(STEP_ENC_EA);
    if(value_b){
        if(value_a){
            encoder_step++;
        } else {
            encoder_step--;
        }
    } else {
        if(value_a){
            encoder_step++;
        } else {
            encoder_step--;
        }
    }
}

void core1_irq(uint gpio, uint32_t events){
    int value = events == GPIO_IRQ_EDGE_RISE ? 1 : 0;
    if(gpio == STEP_ENC_EA){
        doEncoderA(value);
    } else if (gpio == STEP_ENC_EB) {
        doEncoderB(value);
    }
    
}

void core1_entry(){
    // STEPPER Encoder init
    gpio_init(STEP_ENC_EA);
    gpio_set_dir(STEP_ENC_EA, GPIO_IN);

    gpio_init(STEP_ENC_EB);
    gpio_set_dir(STEP_ENC_EB, GPIO_IN);

    gpio_set_irq_enabled_with_callback(STEP_ENC_EA,GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &core1_irq);
    gpio_set_irq_enabled(STEP_ENC_EB, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    int lastVal = 0;
    while(true){
        sleep_ms(1000);
        if(lastVal != encoder_step){
            lastVal = encoder_step;
            char out[12];
            sprintf(out, "%d", lastVal);
            puts(out);
        }

    }


    //bool last_ea = gpio_get(STEP_ENC_EA);


    while (false) {
        bool current_ea = gpio_get(STEP_ENC_EA);
        bool current_eb = gpio_get(STEP_ENC_EB);
        if(last_ea != current_ea){
            if(current_eb == current_ea){
                encoder_step++;
            } else {
                encoder_step--;
            }
            if(encoder_step >= 1000){
                encoder_step = 0;
            } else if(encoder_step < 0){
                encoder_step = 999;
            }
            char output[12];
            sprintf(output, "%d", encoder_step);
            puts(output);
        }
        last_ea = current_ea;

    }

}

void doStep(int time){
    if (time == 0){
        time = stepTime;
    }
    if(currentDirection == right){
        currentStep--;
    } else {
        currentStep++;
    }

    if(currentStep >= totalSteps){
        currentStep = 0;
    } else if(currentStep < 0){
        currentStep = totalSteps - 1;
    }

    gpio_put(PUL_PIN, true);
    sleep_us(time / 2);
    gpio_put(PUL_PIN, false);
    sleep_us(time / 2);

}

void setDirection(enum DIR direction){
    gpio_put(DIR_PIN, direction);
    currentDirection = direction;

}

int numberToStep(int number){
    return totalSteps / totalNumbers * number;
}

void goToNumber(uint number, enum DIR direction, enum motorRamp ramp){
    setDirection(direction);
    
    int goalStep = numberToStep(number);
    int deltaSteps = 0;
    if (number == currentNumber){
        deltaSteps = totalSteps;
    }
    else if (goalStep < currentStep && direction == left){
        deltaSteps = totalSteps - (currentStep - goalStep); //currentStep - goalStep;
    }
    else if (goalStep > currentStep && direction == right){
        deltaSteps = totalSteps - (goalStep - currentStep);//goalStep - currentStep;
    }
    else if (goalStep > currentStep && direction == left){
        deltaSteps = goalStep - currentStep;        //totalSteps - goalStep + currentStep;
    }
    else if (goalStep < currentStep && direction == right){
        deltaSteps = currentStep - goalStep;        //totalSteps - currentStep + goalStep;
    }



    if(deltaSteps > 5 * totalSteps / totalNumbers){
        if (ramp == startstop) {
            for (int i = 0; i < deltaSteps; i++){
                if(i < 0.3*deltaSteps){
                    doStep(2 * 75 - 75/(0.3 * deltaSteps) * i);
                } else if (i >= deltaSteps - 0.3*deltaSteps){
                    doStep(2*75 - 75/(0.3*deltaSteps) * (deltaSteps - i) );
                } 
                else {
                    doStep(0);
                }
                
            }
        }
        else if (ramp == start) {
            for (int i = 0; i < deltaSteps; i++){
                if(i < 0.3*deltaSteps){
                    doStep(2 * 75 - 75/(0.3 * deltaSteps) * i);
                } else {
                    doStep(0);
                }
                
            }
        }
        else if (ramp == stop){
            for (int i = 0; i < deltaSteps; i++){
                if (i >= deltaSteps - 0.3*deltaSteps){
                    doStep(2*75 - 75/(0.3*deltaSteps) * (deltaSteps - i) );
                } 
                else {
                    doStep(0);
                }
                
            }
        } else if (ramp == normal){
            for (int i = 0; i < deltaSteps; i++){
                doStep(0);
            }
        }
    } else {
        for (int i = 0; i < deltaSteps; i++){
            doStep(stepTime + 0.25 * stepTime);
        }
    }
    
    currentNumber = number;
}

int turnRounds(int rounds, enum DIR direction){
    setDirection(direction);
    spikes = 0;
    enum motorRamp ramp = start;
    for (int i = 0; i < rounds; i++){
        if(i > 0 && i < rounds -1){
            ramp = normal;
        } else if(i == rounds - 1) {
            ramp = stop;
        }
        goToNumber(currentNumber, direction, ramp);
    }
    return spikes;
}

void enc_sw_irq_wrap(uint gpio, uint32_t events){
    uint32_t currentTime = to_ms_since_boot(get_absolute_time());
    if(currentTime - enc_sw_last_pressed > 500){
        uart_puts(uart0, "enc_sw_button pressed\n\r");
        doCorrection = false;
        enc_sw_last_pressed = currentTime;
    }
}

void doEncoderCorrection(){

    int lastCLK = true;

    while(doCorrection){
        int newCLK = gpio_get(ENC_CLK_PIN);
        int newDT = gpio_get(ENC_DT_PIN);
        if(lastCLK != newCLK){
            if(newDT == lastCLK){
                uart_puts(uart0, "Clockwise\n\r");      
                setDirection(right);
                doStep(0);
                doStep(0);
                doStep(0);

            } else {
                uart_puts(uart0, "Counterclockwise\n\r");
                setDirection(left);
                doStep(0);
                doStep(0);
                doStep(0);
            }
        }
        lastCLK = gpio_get(ENC_CLK_PIN);
        sleep_ms(10);
    }
    uart_puts(uart0, "Finished initialisation\n\r");

    currentStep = 0;
}

void hall_irq_wraper(uint gpio, uint32_t events){
    spikes++;
    //gpio_put(LED_PIN, spikes % 2);
}

void waitForSw(){
    doCorrection = true;
    while(doCorrection){
        sleep_ms(5);
    }
}


int main(){

    bi_decl(bi_program_description("Tresor - Controller"));
    bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED"));

    stdio_init_all();

    //multicore_launch_core1(core1_entry);

    //LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    bool ledStatus = true;

    //direction - pin
    gpio_init(DIR_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_set_pulls(DIR_PIN, true, false);
    gpio_put(DIR_PIN, true);

    //pul - pin
    gpio_init(PUL_PIN);
    gpio_set_dir(PUL_PIN, GPIO_OUT);
    gpio_set_pulls(PUL_PIN, true, false);
    
    //uart init
    uart_init(uart0, 115200);
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

    // hall  init
    gpio_init(HALL_PWR_PIN);
    gpio_set_dir(HALL_PWR_PIN, GPIO_OUT);
    gpio_set_pulls(HALL_PWR_PIN, true, false);
    gpio_put(HALL_PWR_PIN, 1);

    gpio_init(HALL_SIGNAL_PIN);
    gpio_set_dir(HALL_SIGNAL_PIN, GPIO_IN);
    gpio_set_pulls(HALL_SIGNAL_PIN, true, false);

    // Encoder init 
    gpio_init(ENC_CLK_PIN);
    gpio_set_dir(ENC_CLK_PIN, GPIO_IN);
    gpio_set_pulls(ENC_CLK_PIN, true, false);

    gpio_init(ENC_DT_PIN);
    gpio_set_dir(ENC_DT_PIN, GPIO_IN);
    gpio_set_pulls(ENC_DT_PIN, true, false);

    // setuo encoder switch gpio
    gpio_init(ENC_SW_PIN);
    gpio_set_dir(ENC_SW_PIN, GPIO_IN);
    gpio_set_pulls(ENC_SW_PIN, true, false);

    gpio_set_irq_enabled_with_callback(ENC_SW_PIN, GPIO_IRQ_EDGE_FALL, true, &enc_sw_irq_wrap); 
    // manual turn to zero
    doEncoderCorrection();
    // precision controll with manual continuation
    goToNumber(20, right, normal);
    waitForSw();
    goToNumber(0, left, normal);
    waitForSw();
    goToNumber(20,left, normal);
    waitForSw();
    goToNumber(0, right, normal);
    waitForSw();  
    
    // disable encoder switch isr
    gpio_set_irq_enabled(ENC_SW_PIN, 0, false);

    // enable hall sensor isr
    gpio_set_irq_enabled_with_callback(HALL_SIGNAL_PIN, GPIO_IRQ_EDGE_FALL, true, &hall_irq_wraper);
    
    sleep_ms(2000);


    for(int number1 = 0; number1 < totalNumbers; number1++){
        for (int number2 = 0; number2 < 100; number2++) {
            
            // Clearing
            gpio_put(LED_PIN, 0);
            turnRounds(4, left);
            sleep_ms(500);
            // 3 x rechts
            gpio_put(LED_PIN, 1);
            goToNumber(number1, right, start);
            goToNumber(number1, right, normal);
            goToNumber(number1, right, stop);
            sleep_ms(500);
            // 2 x links
            goToNumber(number2, left, start);
            goToNumber(number2, left, stop);
            sleep_ms(500);
            // 2 x rechts -> abcheken ob offen
            int spikes = turnRounds(2, right);

            char output[32];
            sprintf(output, "[%d, %d, %d]\n\r", number1, number2, spikes);
            uart_puts(uart0, output);
            if (spikes < 8){
                char spikeWarnOut[32];
                sprintf(spikeWarnOut, "Only %d spikes!!!\n\r", spikes);
                uart_puts(uart0, spikeWarnOut);
            }
            sleep_ms(500);

        }
    }
}
