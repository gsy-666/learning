package com.gsy.machinecontrol.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.gsy.machinecontrol.ui.theme.Background
import com.gsy.machinecontrol.ui.theme.CardBackground
import com.gsy.machinecontrol.ui.theme.GreenPrimary
import com.gsy.machinecontrol.ui.theme.TextPrimary
import com.gsy.machinecontrol.viewmodel.MachineViewModel

@Composable
fun DeviceControlScreen(viewModel: MachineViewModel) {
    val automation by viewModel.automationMode.collectAsState()
    val ventilation by viewModel.ventilationState.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Background)
            .padding(16.dp)
    ) {
        Text("设备控制", fontSize = 20.sp, fontWeight = FontWeight.Bold, color = TextPrimary)
        Spacer(modifier = Modifier.height(16.dp))

        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = CardBackground),
            elevation = CardDefaults.cardElevation(defaultElevation = 2.dp),
            shape = RoundedCornerShape(16.dp)
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                ControlRow("通风设备", if (ventilation) "自动模式" else "关闭", ventilation) {
                    viewModel.toggleVentilation(it)
                }
                HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp), color = Color(0xFFEEEEEE))
                ControlRow("遮阳系统", "关闭", false) { /* Empty mock */ }
                HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp), color = Color(0xFFEEEEEE))
                ControlRow("加热设备", "自动模式", automation) { 
                    viewModel.toggleAutomation(it)
                }
            }
        }
    }
}

@Composable
fun ControlRow(title: String, status: String, isChecked: Boolean, onCheckedChange: (Boolean) -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(title, fontWeight = FontWeight.Bold, fontSize = 16.sp, color = TextPrimary)
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(status, color = Color.Gray, fontSize = 14.sp)
            Spacer(modifier = Modifier.width(8.dp))
            Switch(
                checked = isChecked,
                onCheckedChange = onCheckedChange,
                colors = SwitchDefaults.colors(
                    checkedThumbColor = Color.White,
                    checkedTrackColor = GreenPrimary
                )
            )
        }
    }
}
