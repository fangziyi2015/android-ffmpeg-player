package com.fm.fmplayer.utils

object TimeFormatUtil {
    /**
     * 这段代码定义了一个函数convertSecondsToTimeFormat，它接收一个Long类型的参数seconds，
     * 表示视频的时长（秒）。函数内部首先计算出总时长包含的小时数、剩余分钟数以及剩余秒数，
     * 然后使用String.format方法将这些数值格式化为两位数（不足两位前面补0），
     * 最后以小时:分钟:秒的格式返回字符串。
     * 例如，如果你有一个视频时长为3725秒，调用这个函数会返回字符串01:02:05。
     */
    fun convertSecondsToTimeFormat(seconds: Long): String {
        val hours = seconds / 3600
        val minutes = (seconds % 3600) / 60
        val remainingSeconds = seconds % 60

        // 格式化输出，确保单个数字前有0填充
        return String.format("%02d:%02d:%02d", hours, minutes, remainingSeconds)
    }

    fun defaultTimeFormat(): String {
        return "00:00:00"
    }

}