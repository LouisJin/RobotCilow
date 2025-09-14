#include "lvgl.h"
#include "esp_log.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "d_lcd.h"

static const char *TAG = "lvgl_api";

// emoji表情序列
// 正常
static char* EMOJI_NORMAL_SEQ_PATH[] = {
    "S:/blink_once.gif", 
    NULL
};
// 疑惑
static char* EMOJI_DOUBT_SEQ_PATH[] = {
    "S:/blink_twice.gif",
    NULL
};
// 思考
static char* EMOJI_THINK_SEQ_PATH[] = {
    "S:/look_lt_in.gif",
    "S:/look_lt_do.gif",
    "S:/look_lt_do.gif",
    "S:/look_lt_do.gif",
    "S:/look_lt_out.gif",
    NULL
};
// 感知
static char* EMOJI_PERCEPT_SEQ_PATH[] = {
    "S:/look_rt_in.gif",
    "S:/look_rt_do.gif",
    "S:/look_rt_do.gif",
    "S:/look_rt_do.gif",
    "S:/look_rt_out.gif",
    NULL
};
// 恐慌
static char* EMOJI_PANIC_SEQ_PATH[] = {
    "S:/panic_in.gif", 
    "S:/panic_do.gif", 
    "S:/panic_do.gif", 
    "S:/panic_do.gif", 
    "S:/panic_out.gif",
    NULL
};
// 不屑
static char* EMOJI_DISDAIN_SEQ_PATH[] = {
    "S:/disdain_in.gif",
    "S:/disdain_do.gif",
    "S:/disdain_do.gif",
    "S:/disdain_do.gif",
    "S:/disdain_out.gif",
    NULL
};
// 生气
static char* EMOJI_ANGRY_SEQ_PATH[] = {
    "S:/angry_in.gif", 
    "S:/angry_do.gif", 
    "S:/angry_do.gif", 
    "S:/angry_do.gif", 
    "S:/angry_out.gif",
    NULL
};
// 兴奋
static char* EMOJI_EXCITE_SEQ_PATH[] = {
    "S:/excite_in.gif", 
    "S:/excite_do.gif", 
    "S:/excite_do.gif", 
    "S:/excite_do.gif", 
    "S:/excite_out.gif",
    NULL
};
// 悲伤
static char* EMOJI_SAD_SEQ_PATH[] = {
    "S:/sad_in.gif", 
    "S:/sad_do.gif", 
    "S:/sad_do.gif", 
    "S:/sad_do.gif", 
    "S:/sad_out.gif",
    NULL
};

// gif seq序列结构体
typedef struct {
    char** seq_path;        // 序列路径数组
    uint8_t seq_index;     // 当前播放序列索引
} gif_seq_t;

// 当前gif播放对象
static lv_obj_t* emoji_gif = NULL;
// emoji播放定时器
static TimerHandle_t emoji_timer = NULL;

void play_gif_seq(gif_seq_t* gif_seq);

// gif每一序列播放完毕回调
void gif_playback_complete_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        gif_seq_t* gif_seq = (gif_seq_t*)lv_event_get_user_data(e);
        gif_seq->seq_index++;
        if (emoji_gif != NULL) {
            lv_obj_del_async(emoji_gif);
            emoji_gif = NULL;
        }
        play_gif_seq(gif_seq);
    }
}

// 播放GIF序列
void play_gif_seq(gif_seq_t* gif_seq) {
    if (gif_seq == NULL) {
        ESP_LOGW(TAG, "No current emoji sequence set");
        return;
    }
    // 所有序列播放完毕，执行回收代码
    if (gif_seq->seq_path[gif_seq->seq_index] == NULL) {
        ESP_LOGI(TAG, "Emoji sequence completed");
        gif_seq->seq_index = 0;
        free(gif_seq);
        // 关闭刷新
        disp_disable_update();
        return;
    }
    // 开启刷新
    disp_enable_update();
    ESP_LOGI(TAG, "Playing emoji sequence index: %d, path: %s", gif_seq->seq_index, gif_seq->seq_path[gif_seq->seq_index]);
    
    emoji_gif = lv_gif_create(lv_screen_active());
    lv_gif_set_src(emoji_gif, gif_seq->seq_path[gif_seq->seq_index]);
    lv_gif_set_loop_count(emoji_gif, 1); // 设置只播放一次
    lv_obj_center(emoji_gif);
    lv_obj_add_event_cb(emoji_gif, gif_playback_complete_cb, LV_EVENT_ALL, gif_seq);
}

gif_seq_t* create_gif_seq(char* seq_path[]) {
    if (seq_path == NULL || seq_path[0] == NULL) {
        ESP_LOGW(TAG, "No emoji sequence set");
        return NULL;
    }
    gif_seq_t* gif_seq = malloc(sizeof(gif_seq_t));
    gif_seq->seq_path = seq_path;
    gif_seq->seq_index = 0;
    return gif_seq;
}

void emoji_play(EMOTE_TYPE type) {
    // 每次来新表情，删除掉之前的表情
    lv_obj_clean(lv_scr_act());
    switch (type) {
        case EMOTE_NORMAL:
            play_gif_seq(create_gif_seq(EMOJI_NORMAL_SEQ_PATH));
            break;
        case EMOTE_DOUBT:
            play_gif_seq(create_gif_seq(EMOJI_DOUBT_SEQ_PATH));
            break;
        case EMOTE_THINK:
            play_gif_seq(create_gif_seq(EMOJI_THINK_SEQ_PATH));  
            break;
        case EMOTE_PERCEPT:
            play_gif_seq(create_gif_seq(EMOJI_PERCEPT_SEQ_PATH));
            break;
        case EMOTE_PANIC:
            play_gif_seq(create_gif_seq(EMOJI_PANIC_SEQ_PATH));
            break;
        case EMOTE_DISDAIN:
            play_gif_seq(create_gif_seq(EMOJI_DISDAIN_SEQ_PATH));
            break;
        case EMOTE_ANGRY:
            play_gif_seq(create_gif_seq(EMOJI_ANGRY_SEQ_PATH));
            break;
        case EMOTE_EXCITE:
            play_gif_seq(create_gif_seq(EMOJI_EXCITE_SEQ_PATH));
            break;
        case EMOTE_SAD:
            play_gif_seq(create_gif_seq(EMOJI_SAD_SEQ_PATH));
            break;
        default:
            ESP_LOGW(TAG, "Unsupported emotion type");
            break;
    }
}

void emoji_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "auto play emoji blink");
    emoji_play(EMOTE_NORMAL);
}

// 表情初始化，默认一段时间，眨一下眼
// !!! 一定要在menuconfig中配置 TIMER_TASK_STACK_DEPTH >= 4096 防止堆栈溢出
void emoji_init(void) {
    // 创建定时器
    emoji_timer = xTimerCreate(
        "EmojiTimer",           // 定时器名称
        pdMS_TO_TICKS(EMOJI_BLINK_INTEVEL),      
        pdTRUE,                    // 自动重载（周期性定时器）
        (void *)0,                 // 定时器ID（可自定义）
        emoji_timer_callback             // 回调函数
    );
    if (emoji_timer == NULL) {
        ESP_LOGE(TAG, "create emoji timer fail...");
        return;
    }
    // 启动定时器
    if (xTimerStart(emoji_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "start emoji timer fail...");
        xTimerDelete(emoji_timer, 0);
    } else {
        ESP_LOGI(TAG, "emoji timer start, %d ms execute", EMOJI_BLINK_INTEVEL);
    }
}