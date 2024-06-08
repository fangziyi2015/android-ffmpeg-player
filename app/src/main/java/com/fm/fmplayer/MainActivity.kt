package com.fm.fmplayer

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.Environment
import android.widget.TextView
import com.fm.fmplayer.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var fmPlayer: FmPlayer
    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        fmPlayer = FmPlayer().apply {
            dataSource = File(Environment.getExternalStorageDirectory(),"trailer.mp4").absolutePath
            lifecycle.addObserver(this)
            prepareCallback = {
                runOnUiThread {
                    binding.tvStatusInfo.text = "初始化成功"
                }

                start()
            }

            errorCallback = { msg ->
                runOnUiThread {
                    binding.tvStatusInfo.text = msg
                }
            }
        }

        binding.apply {
            fmPlayer.setSurfaceView(surfaceView)
        }
    }

}