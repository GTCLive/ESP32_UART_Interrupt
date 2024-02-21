
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_intr_alloc.h"
#include "soc/interrupts.h"
#include "hal/uart_hal.h"

#define U0TX_GPIO			(43)		// ESP32-S3
#define U0RX_GPIO			(44)		// ESP32-S3
#define UART_NUM			UART_NUM_0	// ESP32-S3
#define RXFULL_THRESHOLD		(12)		// n of expected rx bytes.
#define RXBUF_SIZE			(256)		// 256 minimum

uint16_t rx_fifo_len = 0;
uint8_t rxbuf[RXFULL_THRESHOLD+1] = {0};


void uart_task(void *pvParameter)
{
	printf("Received %d bytes: ", rx_fifo_len);
	for(uint16_t i=0; i < rx_fifo_len; i++){
		printf("0x%x ", rxbuf[i]);
	}
	printf("\n");
	//printf("Received %d bytes: %s ", rx_fifo_len, rxbuf);

	// Clear isr status and ring buffer from here.
	uart_clear_intr_status(UART_NUM, UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR);
	uart_flush(UART_NUM);
    vTaskDelete(NULL);
}


void IRAM_ATTR uart_intr_handler(void *arg)
{
	rx_fifo_len = UART0.status.rxfifo_cnt;
	if(rx_fifo_len == RXFULL_THRESHOLD)
	{
		memset(&rxbuf, 0, rx_fifo_len);
		for(uint16_t i=0; i < rx_fifo_len; i++){
			rxbuf[i] = UART0.fifo.rxfifo_rd_byte;
		}
		xTaskCreate(uart_task, "uart_task", 4096, NULL, 2, NULL);
	}
}


void app_main()
{
	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM, U0TX_GPIO, U0RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM, RXBUF_SIZE, 0, 0, NULL, ESP_INTR_FLAG_SHARED));
	ESP_ERROR_CHECK(uart_enable_rx_intr(UART_NUM));

	// Overwrite ISR with custom handler
	intr_handle_t uart_intr_handle;
	uart_intr_config_t isr_cfg = {
		    .intr_enable_mask = UART_RXFIFO_FULL_INT_ENA,
		    .rxfifo_full_thresh = RXFULL_THRESHOLD,
	};
	ESP_ERROR_CHECK(uart_intr_config(UART_NUM,&isr_cfg));
	ESP_ERROR_CHECK(esp_intr_alloc(ETS_UART0_INTR_SOURCE, ESP_INTR_FLAG_SHARED, uart_intr_handler, NULL, &uart_intr_handle));

    return;
}
