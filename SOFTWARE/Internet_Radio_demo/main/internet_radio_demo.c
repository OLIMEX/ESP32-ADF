/* 
 * 
 * 
 * 
 * Internet Radio Demo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#define tft_display

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "aac_decoder.h"
#include "mp3_decoder.h"


#include "esp_peripherals.h"
#include "periph_touch.h"
#include "periph_button.h"
#include "input_key_service.h"
#include "periph_adc_button.h"
#include "periph_wifi.h"
#include "board.h"
#include "periph_button.h"

#ifdef tft_display
#define CONFIG_EXAMPLE_DISPLAY_TYPE 2
#define SPI_BUS TFT_HSPI_HOST

#include "tft.h"
#include "tftspi.h"


#endif


#define CURRENT 0
#define NEXT    1
#define PRESSET_RADIO 3
#define VOLUME_MUTED 10
#define AUDIO_HAL_VOL_DEFAULT 40

TimerHandle_t xTimer;
uint8_t busy = 0;
static const char *TAG = "INTERNET_RADIO_EXAMPLE";


//#define BULGARIAN
#define NEDERLAND

#ifdef BULGARIAN
#define FORMAT_AAC
static const char *radio[] = {
	"http://stream.radioreklama.bg:80/radio1.aac","Radio 1",
    "http://stream.radioreklama.bg:80/radio1rock.aac","Radio 1 Rock",	
//	"http://193.108.24.6:8000/zrock","Z-Rock",
	"http://stream.radioreklama.bg:80/bgradio.aac","BG Radio",
//	"http://stream.radioreklama.bg/nrj.aac.m3u","Energy",
	"http://stream.radioreklama.bg:80/energy-90s.aac","Energy 90s",
	"https://bss.neterra.tv/rtplive/vitosharadio_live.stream/playlist.m3u8","Vitosha",
	"https://bss.neterra.tv/rtplive/thevoiceradio_live.stream/playlist.m3u8","The Voice",
	"https://bss.neterra.tv/rtplive/magicfmradio_live.stream/playlist.m3u8","Magic FM",
//    "http://46.10.150.243:80/njoy.mp3","N-Joy",
    "http://stream.radioreklama.bg:80/city.aac","City",	
//    "http://darikradio.by.host.bg:8000/S2-128","Darik",
	"http://lb.blb.cdn.bg:2032/fls/Horizont.stream/playlist.m3u8","BNR Horizont",
	"http://lb.blb.cdn.bg:2032/fls/HrBotev.stream/playlist.m3u8","BNR Hristo Botev",
};
#endif
#ifdef NEDERLAND
#define FORMAT_AAC
static const char *radio[] = {
	"http://stream.slam.nl/slamaac","SLAM! Live",
    "http://playerservices.streamtheworld.com/api/livestream-redirect/TLPSTR10AAC.aac","538 Dance Radio",	
	"http://playerservices.streamtheworld.com/api/livestream-redirect/RADIO538AAC.aac","538",
	"http://playerservices.streamtheworld.com/api/livestream-redirect/TLPSTR05AAC.aac","538 Global Dance Chart",
	"http://playerservices.streamtheworld.com/api/livestream-redirect/TLPSTR11AAC.aac","538 Hitzone",
	"http://playerservices.streamtheworld.com/api/livestream-redirect/TLPSTR19AAC.aac","538 Ibiza Radio",
    "http://playerservices.streamtheworld.com/api/livestream-redirect/TLPSTR09AAC.aac","538 Non-Stop",
    "http://playerservices.streamtheworld.com/api/livestream-redirect/TLPSTR16AAC.aac","538 Party",	
    "http://playerservices.streamtheworld.com/api/livestream-redirect/VERONICAAAC.aac","Radio Veronica",
    "http://playerservices.streamtheworld.com/api/livestream-redirect/SRGSTR12AAC.aac","Veronica 80's Hits",
    "http://playerservices.streamtheworld.com/api/livestream-redirect/SRGSTR13AAC.aac","Veronica 90's Hits",
    "http://playerservices.streamtheworld.com/api/livestream-redirect/SRGSTR14AAC.aac","Veronica 00's Top 500",
    "http://stream.radiocorp.nl/web10_aac","Slam! Non-Stop",
    "http://icecast.omroep.nl/radio1-bb-aac","Radio 1",
};
#endif

#define RADIO_COUNT (sizeof(radio)/sizeof(char*))/2
int radio_index=0;
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_writer,
#ifdef FORMAT_AAC 
     aac_decoder,
     #endif
#ifdef FORMAT_MP3
     mp3_decoder,
#endif     
	 http_stream_reader;
				
    audio_board_handle_t board_handle;
    audio_event_iface_handle_t evt;
	int player_volume = AUDIO_HAL_VOL_DEFAULT;
	static struct tm* tm_info;
static char tmp_buff[64];
static uint8_t tune_request = 255;
static time_t time_now, time_last = 0;
static const char *file_fonts[3] = {"/spiffs/fonts/DotMatrix_M.fon", "/spiffs/fonts/Ubuntu.fon", "/spiffs/fonts/Grotesk24x48.fon"};
//---------------------
/*
static void _dispTime()
{
	Font curr_font = cfont;
    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

    time(&time_now);
	time_last = time_now;
	tm_info = localtime(&time_now);
	sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
	TFT_print(tmp_buff, CENTER, _height-TFT_getfontheight()-5);

    cfont = curr_font;
} */	
static void disp_volume(int volume){
	TFT_resetclipwin();
	_fg = TFT_YELLOW;
	_height = 320;
	_bg = (color_t){ 64, 64, 64 };
	if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);
	TFT_fillRect(1, _height-TFT_getfontheight()-8, _width-2 , TFT_getfontheight()+7, _bg);
	TFT_fillRect(1, _height-TFT_getfontheight()-8, volume * 2, TFT_getfontheight()+7, TFT_YELLOW);
	TFT_drawRect(0, _height-TFT_getfontheight()-9, _width-1, TFT_getfontheight()+8, TFT_CYAN);
	//TFT_print("VOLUME", CENTER, 4);
	ESP_LOGI(TAG, "[ * ]  Volume bar set to %d",volume);
}
	
static void disp_header(char *info)
{
	//TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();

	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };

    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);
	TFT_fillRect(0, 0, _width-1, TFT_getfontheight()+8, _bg);
	TFT_drawRect(0, 0, _width-1, TFT_getfontheight()+8, TFT_CYAN);
	
	disp_volume(player_volume);

	TFT_print(info, CENTER, 4);

	for (int r = 0; r < RADIO_COUNT; r++)
	{
		TFT_setclipwin(0, TFT_getfontheight()+9 + 18 * r, _width-1, _height-TFT_getfontheight()-10);
		TFT_print(radio[(r<<1)|1], CENTER, 4);
		
	}
	
	
	//_dispTime();

	_bg = TFT_BLACK;
	
}	

int _http_stream_event_handle(http_stream_event_msg_t *msg)
{

    if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS) {
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_TRACK) {
        return http_stream_next_track(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST) {
        return http_stream_restart(msg->el);
    }
    return ESP_OK;

}

void tune_radio(int radio_index){
static uint8_t curent_radio;	
	busy = 1;
	if (radio_index == curent_radio) return;
	curent_radio = radio_index;
			ESP_LOGW(TAG, "[ * ] Tune stream");
            audio_pipeline_stop(pipeline);
            audio_pipeline_wait_for_stop(pipeline);
#ifdef FORMAT_AAC            
            audio_element_reset_state(aac_decoder);
#endif
#ifdef FORMAT_MP3
            audio_element_reset_state(mp3_decoder);
#endif            
            audio_element_reset_state(i2s_stream_writer);
            audio_pipeline_reset_ringbuffer(pipeline);
            audio_pipeline_reset_items_state(pipeline);
            audio_element_set_uri(http_stream_reader, radio[radio_index<<1]);
            audio_pipeline_run(pipeline);
            // _fg = TFT_CYAN;
             disp_header(radio[((radio_index<<1)|1)]);
			//TFT_print(radio[((radio_index<<1)|1)], CENTER, CENTER);
            ESP_LOGW(TAG, "[ %s ] %s",radio[((radio_index<<1)|1)],radio[radio_index<<1]);
            audio_element_info_t music_info = {0};
#ifdef FORMAT_AAC            
			audio_element_getinfo(aac_decoder, &music_info);
#endif
#ifdef FORMAT_MP3
			audio_element_getinfo(mp3_decoder, &music_info);
#endif			
			ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);
			audio_element_setinfo(i2s_stream_writer, &music_info);
			i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
			vTaskDelay(500 / portTICK_RATE_MS);
	busy = 0;		
}
 void vTimerCallback( TimerHandle_t xTimer )
 {

 uint32_t ulCount;

    configASSERT( xTimer );

    ulCount = ( uint32_t ) pvTimerGetTimerID( xTimer );
    ulCount++;
    vTimerSetTimerID( xTimer, ( void * ) ulCount );
    
    
  //  gpio_set_level(get_green_led_gpio(), ulCount & 1);
    int tx, ty, tz;        
      if (TFT_read_touch(&tx, &ty, 0)){
		 ESP_LOGI(TAG, "[ * ] Display TOUCH : x=%d y=%d", tx, ty);
		 
		 
		 if (ty > 280) //volume
		 {
			if ((player_volume > 0) && (tx < 90)) player_volume--;
			if ((player_volume < 100) && (tx > 110)) player_volume++;
			
			audio_hal_set_volume(board_handle->audio_hal, player_volume);
			disp_volume(player_volume);
		}
		 else if (!busy) tune_request = ty / 18;
		 

		}
 }
void app_main(void)
{
	int ret;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
 #ifdef tft_display
     
      // ==== Set display type
	 tft_disp_type = DISP_TYPE_ILI9341;
      ESP_LOGI(TAG, "[ * ]  Initialize tft display %d ILI9341",CONFIG_EXAMPLE_DISPLAY_TYPE);
      max_rdclock = 8000000;
      
      //
      TFT_PinsInit();

      ESP_LOGI(TAG, "[ * ]  pins MOSI = %d, DC = %d, CLK = %d, CS = %d",PIN_NUM_MOSI,PIN_NUM_DC,PIN_NUM_CLK,PIN_NUM_CS);
      
     
      
    spi_lobo_device_handle_t spi;
	
    spi_lobo_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,				// set SPI MISO pin
        .mosi_io_num=PIN_NUM_MOSI,				// set SPI MOSI pin
        .sclk_io_num=PIN_NUM_CLK,				// set SPI CLK pin
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
		.max_transfer_sz = 6*1024,
    };
    spi_lobo_device_interface_config_t devcfg={
        .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
        .mode=0,                                // SPI mode 0
        .spics_io_num=-1,                       // we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,           // external CS pin
		.flags=LB_SPI_DEVICE_HALFDUPLEX,        // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
    }; 
		// we are using MISO as DC wire set MISO as GPIO
		gpio_pad_select_gpio(PIN_NUM_DC);
		gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
		gpio_set_level(PIN_NUM_DC, 0);
 		gpio_pad_select_gpio(PIN_NUM_CS);
		gpio_set_direction(PIN_NUM_CS, GPIO_MODE_OUTPUT);
		gpio_set_level(PIN_NUM_DC, 1);     
      ESP_LOGI(TAG, "[ * ]  Display %d ILI9341 initialized",CONFIG_EXAMPLE_DISPLAY_TYPE);
      
 
	
	   vTaskDelay(500 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "SPI=%d, Pins used: miso=%d, mosi=%d, sck=%d, cs=%d, dc=%d", SPI_BUS, PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS, PIN_NUM_DC);
#if USE_TOUCH > TOUCH_TYPE_NONE
    ESP_LOGI(TAG, " Touch CS: %d", PIN_NUM_TCS);
#endif
//	printf("==============================\r\n\r\n");
	
// ==================================================================
	// ==== Initialize the SPI bus and attach the LCD to the SPI bus ====

	ret=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
    assert(ret==ESP_OK);
	ESP_LOGI(TAG, "SPI: display device added to spi bus (%d)", SPI_BUS);
	disp_spi = spi;

	// ==== Test select/deselect ====
	ret = spi_lobo_device_select(spi, 1);
    assert(ret==ESP_OK);
	ret = spi_lobo_device_deselect(spi);
    assert(ret==ESP_OK);

	ESP_LOGI(TAG, "SPI: attached display device, speed=%u", spi_lobo_get_speed(spi));
	ESP_LOGI(TAG, "SPI: bus uses native pins: %s", spi_lobo_uses_native_pins(spi) ? "true" : "false");	
    font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
	// ================================
	// ==== Initialize the Display ====

	ESP_LOGI(TAG, "SPI: display init...");
	TFT_display_init();
	
	// ---- Detect maximum read speed ----
	max_rdclock = find_rd_speed();
	printf("SPI: Max rd speed = %u\r\n", max_rdclock);

    // ==== Set SPI clock used for display operations ====
	spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
	ESP_LOGI(TAG, "SPI: Changed speed to %u", spi_lobo_get_speed(spi));


    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(PORTRAIT);
	TFT_setFont(DEFAULT_FONT, NULL);
	TFT_resetclipwin();
	TFT_fillScreen(TFT_BLACK);
		

	
#endif      

     tcpip_adapter_init();
    
    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    
  
	ESP_LOGI(TAG, "[ 1.1 ] Start audio codec chip");
    board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, AUDIO_HAL_VOL_DEFAULT);    

    ESP_LOGI(TAG, "[2.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[2.1] Create http stream to read data");
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    http_stream_reader = http_stream_init(&http_cfg);

    ESP_LOGI(TAG, "[2.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);


#ifdef FORMAT_AAC
    ESP_LOGI(TAG, "[2.3] Create aac decoder to decode aac file");
    aac_decoder_cfg_t aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
    aac_decoder = aac_decoder_init(&aac_cfg);

    ESP_LOGI(TAG, "[2.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, http_stream_reader, "http");
    audio_pipeline_register(pipeline, aac_decoder,        "aac");
    audio_pipeline_register(pipeline, i2s_stream_writer,  "i2s");

    ESP_LOGI(TAG, "[2.5] Link it together http_stream-->aac_decoder-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(pipeline, (const char *[]) {"http",  "aac", "i2s"}, 3);
#endif
#ifdef FORMAT_MP3
    ESP_LOGI(TAG, "[2.3] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);

    ESP_LOGI(TAG, "[2.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, http_stream_reader, "http");
    audio_pipeline_register(pipeline, mp3_decoder,        "mp3");
    audio_pipeline_register(pipeline, i2s_stream_writer,  "i2s");

    ESP_LOGI(TAG, "[2.5] Link it together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(pipeline, (const char *[]) {"http",  "mp3", "i2s"}, 3);
#endif
    ESP_LOGI(TAG, "[2.6] Set up  uri (http as http_stream, aac as aac decoder, and default output is i2s)");
    audio_element_set_uri(http_stream_reader, radio[radio_index]);

    
    ESP_LOGI(TAG, "[ 3 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);
    
  
    ESP_LOGI(TAG, "[3.1] Initialize Touch peripheral");
    periph_touch_cfg_t touch_cfg = {
        .touch_mask = BIT(get_input_set_id()) | BIT(get_input_play_id()) | BIT(get_input_volup_id()) | BIT(get_input_voldown_id())  ,
        .tap_threshold_percent = 70,
    };
    esp_periph_handle_t touch_periph = periph_touch_init(&touch_cfg);
    periph_button_cfg_t btn_cfg = {
        .gpio_mask = (1ULL << get_input_rec_id()) | (1ULL << get_input_mode_id()), //REC BTN & MODE BTN
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);

    ESP_LOGI(TAG, "[3.2] Start all peripherals");
    esp_periph_start(set, touch_periph);
    esp_periph_start(set, button_handle);
    
    

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);
    
  //   gpio_set_direction(get_green_led_gpio(), GPIO_MODE_OUTPUT);
     
 #ifdef tft_display
    #if USE_TOUCH == TOUCH_TYPE_STMPE610
    
    
	stmpe610_Init();
	vTaskDelay(10 / portTICK_RATE_MS);
    uint32_t tver = stmpe610_getID();
    ESP_LOGI(TAG, "STMPE touch initialized, ver: %04x - %02x", tver >> 8, tver & 0xFF);
    #endif
			disp_header(radio[((radio_index<<1)|1)]);
			disp_volume(AUDIO_HAL_VOL_DEFAULT);
			

 #endif        
   
    xTimer = xTimerCreate
                   ( "Timer",
                       50 / portTICK_RATE_MS,
                     pdTRUE,
                     ( void * ) 0,
                     vTimerCallback
                   );
              if( xTimerStart( xTimer, 0 ) != pdPASS )
             {
                 /* The timer could not be set into the Active
                 state. */
             }
    //vTaskStartScheduler();
   
   
     
     ESP_LOGW(TAG, "[ * ]     Touch buttons:");
     ESP_LOGW(TAG, "[ * ] *          Volume: Vol- Vol+");
     ESP_LOGW(TAG, "[ * ] *    Next station: <Play>");
     ESP_LOGW(TAG, "[ * ] * Presset station: <Set>");
     
     

     

 

    while (1) {
		
		
		    
		
        audio_event_iface_msg_t msg;
                  
  
        
        esp_err_t ret = audio_event_iface_listen(evt, &msg, 100 / portTICK_RATE_MS); // portMAX_DELAY);

        if (ret != ESP_OK) {
			
			  if (tune_request < RADIO_COUNT)
		      { 
				  tune_radio(tune_request);
				  tune_request = 255;
				  continue;
			  } else
			  {
				tune_request = 255;
            //ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            //gpio_set_level(get_green_led_gpio(), 0);
            continue;
		  }
        } else gpio_set_level(get_green_led_gpio(), 1);

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *) aac_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(aac_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from aac decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        /* restart stream when the first pipeline element (http_stream_reader in this case) receives stop event (caused by reading errors) */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) http_stream_reader
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (int) msg.data == AEL_STATUS_ERROR_OPEN) {
            ESP_LOGW(TAG, "[ * ] Restart stream");
            gpio_set_level(get_green_led_gpio(), 0);
            audio_pipeline_stop(pipeline);
            audio_pipeline_wait_for_stop(pipeline);
            audio_element_reset_state(aac_decoder);
            audio_element_reset_state(i2s_stream_writer);
            audio_pipeline_reset_ringbuffer(pipeline);
            audio_pipeline_reset_items_state(pipeline);
            audio_pipeline_run(pipeline);
            continue;
        }
        
		if ((int)msg.data == get_input_mode_id()) {

			
            if (msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
					ESP_LOGI(TAG, "PERIPH_BUTTON_MODE_LONG_PRESSED");
            
		}
			if (msg.cmd == PERIPH_BUTTON_PRESSED){ 
				ESP_LOGI(TAG, "PERIPH_BUTTON_MODE_PRESSED");

		continue;		
	             
		}
			if ((msg.cmd == PERIPH_BUTTON_RELEASE) || (msg.cmd == PERIPH_BUTTON_LONG_RELEASE)){ 
				ESP_LOGI(TAG, "PERIPH_BUTTON_MODE_RELEASED");
		             
		}
            continue;
        }
        
   		if ((int)msg.data == get_input_rec_id()) {

            if (msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
					ESP_LOGI(TAG, "PERIPH_BUTTON_REC_LONG_PRESSED");
            
		}
              if (msg.cmd == PERIPH_BUTTON_PRESSED){ 
				ESP_LOGI(TAG, "PERIPH_BUTTON_REC_PRESSED");
				
	
		}
		 	if  ((msg.cmd == PERIPH_BUTTON_RELEASE) || (msg.cmd == PERIPH_BUTTON_LONG_RELEASE)){ 
				ESP_LOGI(TAG, "PERIPH_BUTTON_REC_RELEASED");
		             
		}
            continue;
        }
             
            if (msg.source_type == PERIPH_ID_TOUCH
            && msg.cmd == PERIPH_TOUCH_TAP
            && msg.source == (void *)touch_periph) {
				
			
				
				if ((int) msg.data == get_input_play_id()) {
                ESP_LOGI(TAG, "[ * ] [Play] touch tap event");
                gpio_set_level(get_green_led_gpio(), 0);
                if (++radio_index > RADIO_COUNT - 1) {
            radio_index = 0;
        }
					
			tune_radio(radio_index);
            continue;
			
			
                break;
            } else 	if ((int) msg.data == get_input_set_id()) {
                ESP_LOGI(TAG, "[ * ] [Set] touch tap event");
                gpio_set_level(get_green_led_gpio(), 0);
                					
			tune_radio(PRESSET_RADIO);
            continue;
			
			
                break;
            } 
            
            else if ((int) msg.data == get_input_volup_id()) {
                ESP_LOGI(TAG, "[ * ] [Vol+] touch tap event");
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                //disp_volume(player_volume);
                disp_header(radio[((radio_index<<1)|1)]);
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
                
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
            } else if ((int) msg.data == get_input_voldown_id()) {
                ESP_LOGI(TAG, "[ * ] [Vol-] touch tap event");
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                 //disp_volume(player_volume);
                 disp_header(radio[((radio_index<<1)|1)]);
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
               
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
            }
				
			}
        
        
        
    }

    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, http_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, aac_decoder);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(http_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(aac_decoder);
    esp_periph_set_destroy(set);
}
