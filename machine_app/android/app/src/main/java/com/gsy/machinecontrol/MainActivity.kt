package com.gsy.machinecontrol

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import com.gsy.machinecontrol.ui.screens.MainScreen
import com.gsy.machinecontrol.ui.theme.MachineControlTheme

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // 原有的Socket逻辑可以被移到ViewModel或Repository中，这里先替换成Compose。
        setContent {
            MachineControlTheme {
                MainScreen()
            }
        }
    }
}
