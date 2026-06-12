#include <Turbidity.h>
#include <cmath>
// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

TurbiditySensor::TurbiditySensor(ADC_HandleTypeDef* hadc,
                                 GPIO_TypeDef*       controlPort,
                                 uint16_t            controlPin)
    : _hadc(hadc),
      _controlPort(controlPort),
      _controlPin(controlPin)
{
    // Ensure LED starts off - don't assume GPIO reset state
    ledOff();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

float TurbiditySensor::readTurbidity()
{
    // Step 1: get ambient (non-excited) voltage - LED is off
    float vAmbient = sampleMean();

    // Step 2: Enable LED and wait for it to stabilise
    ledOn();

    // Step 3: get excited voltage - V_excited = V_ambient + V_backscatter
    float vExcited = sampleMean();

    // Step 4: Extinguish LED immediately
    ledOff();

    // Step 5:  backscatter by  subtraction
    //         V_backscatter = (V_ambient + V_backscatter) - V_ambient
    float vBackscatter = vExcited - vAmbient;

    // Clamp noise floor: negative values are physically meaningless
    if (vBackscatter < 0.0f) {
        vBackscatter = 0.0f;
    }

    float vBackscatter_mV = countsToMillivolts(vBackscatter);

    // Step 6: Map to NTU via calibration curve
    return backscatterToNtu(vBackscatter_mV);
}

float TurbiditySensor::countsToMillivolts(float counts)
{
    return (counts / static_cast<float>(ADC_MAX_COUNT)) * VREF_MV;
}

// ---------------------------------------------------------------------------
// Private methods
// ---------------------------------------------------------------------------

void TurbiditySensor::ledOn()
{
    HAL_GPIO_WritePin(_controlPort, _controlPin, GPIO_PIN_SET);
    HAL_Delay(LED_SETTLE_MS);
}

void TurbiditySensor::ledOff()
{
    HAL_GPIO_WritePin(_controlPort, _controlPin, GPIO_PIN_RESET);
}

float TurbiditySensor::sampleMean()
{
    uint32_t total = 0;

    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        HAL_ADC_Start(_hadc);
        if (HAL_ADC_PollForConversion(_hadc, 10) == HAL_OK) {
        	total += HAL_ADC_GetValue(_hadc);
        }
        HAL_ADC_Stop(_hadc);
    }

    return static_cast<float>(total) / static_cast<float>(NUM_SAMPLES);
}

float TurbiditySensor::backscatterToNtu(float vBackscatter) const
{
    // V = 67.267 * e^(0.0055 * NTU)
    // Rearranging for NTU: NTU = ln(V / 67.267) / 0.0055
    if (vBackscatter <= 0.0f) return 0.0f;
    float ntu = logf(vBackscatter / 67.267f) / 0.0055f;
    if (ntu <= 0) {
    	ntu = 0;
    }
    return ntu;
}

float TurbiditySensor::readBackscatterVolts(float& vAmbientOut, float& vExcitedOut)
{
    float vAmbient = sampleMean();
    ledOn();
    HAL_Delay(1000);
    float vExcited = sampleMean();
    ledOff();

    vAmbientOut = countsToMillivolts(vAmbient);
    vExcitedOut = countsToMillivolts(vExcited);

    float vBackscatter = vExcited - vAmbient;
    if (vBackscatter < 0.0f) vBackscatter = 0.0f;
    return vBackscatter / 1000.0f;
}
