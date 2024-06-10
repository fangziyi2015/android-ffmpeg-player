package com.fm.fmplayer

import android.annotation.SuppressLint
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.SeekBar
import android.widget.SeekBar.OnSeekBarChangeListener
import android.widget.TextView
import androidx.core.view.isVisible
import com.fm.fmplayer.databinding.ActivityMainBinding
import com.fm.fmplayer.utils.TimeFormatUtil
import java.io.File

class MainActivity : AppCompatActivity(), OnSeekBarChangeListener {

    companion object {
        private const val TAG = "MainActivity"
    }

    private lateinit var fmPlayer: FmPlayer
    private lateinit var binding: ActivityMainBinding
    private var duration = 0L
    private var durationFormat: String = TimeFormatUtil.defaultTimeFormat()
    private var isTouch = false

    @SuppressLint("SetTextI18n")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        fmPlayer = FmPlayer().apply {
            dataSource = File(Environment.getExternalStorageDirectory(), "trailer.mp4").absolutePath
//            dataSource = File(Environment.getExternalStorageDirectory(),"sample.mp4").absolutePath
            // 直播测试地址
//            dataSource = "rtmp://liteavapp.qcloud.com/live/liteavdemoplayerstreamid"
            lifecycle.addObserver(this)
            prepareCallback = {
                duration = this.getDuration()
                Log.i(TAG, "视频总时长，duration: $duration")
                runOnUiThread {
//                    binding.seekbar.max = duration.toInt()
//                    binding.seekbar.max = 100
                    binding.tvStatusInfo.text = "初始化成功"
                    if (duration > 0) {
                        durationFormat = TimeFormatUtil.convertSecondsToTimeFormat(
                            duration
                        )
                        binding.progressView.isVisible = true
                        binding.tvTime.text =
                            TimeFormatUtil.defaultTimeFormat() + "/" + durationFormat
                    }
                }

                start()
            }

            progressCallback = { progress ->
                if (!isTouch) {
                    runOnUiThread {
                        binding.tvTime.text =
                            TimeFormatUtil.convertSecondsToTimeFormat(progress) + "/" + durationFormat

                        Log.i(TAG, "progress 设置完后progressCallback: $progress")
                        val realProgress = progress.toInt() * 100 / duration.toInt()
                        Log.i(TAG, "progress realProgress progressCallback: $realProgress")
                        binding.seekbar.progress = realProgress
                    }
                }
            }

            errorCallback = { msg ->
                runOnUiThread {
                    binding.tvStatusInfo.text = msg
                }
            }
        }

        binding.apply {
            fmPlayer.setSurfaceView(surfaceView)
            seekbar.setOnSeekBarChangeListener(this@MainActivity)
        }
    }

    override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
        if (fromUser) {
            val realProgress = progress * duration.toInt() / 100
            binding.tvTime.text =
                TimeFormatUtil.convertSecondsToTimeFormat(realProgress.toLong()) + "/" + durationFormat
        }
    }

    override fun onStartTrackingTouch(seekBar: SeekBar?) {
        isTouch = true
    }

    override fun onStopTrackingTouch(seekBar: SeekBar?) {
        Handler(Looper.getMainLooper()).postDelayed({
            isTouch = false
        },2000)

        seekBar?.let {
            val progress = it.progress * duration.toInt() / 100
            Log.i(TAG, "onStartTrackingTouch: ${it.progress},progress:$progress")
            fmPlayer.seek(progress)
        }
    }


}