package com.gsy.machinecontrol.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Warning
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
import com.gsy.machinecontrol.ui.theme.TextPrimary
import com.gsy.machinecontrol.ui.theme.TextSecondary
import com.gsy.machinecontrol.viewmodel.MachineViewModel

@Composable
fun AlarmScreen(viewModel: MachineViewModel) {
    val alarms by viewModel.alarms.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Background)
            .padding(16.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("报警", fontSize = 20.sp, fontWeight = FontWeight.Bold, color = TextPrimary)
            Text("全部已读", fontSize = 14.sp, color = TextSecondary)
        }
        Spacer(modifier = Modifier.height(16.dp))

        LazyColumn(verticalArrangement = Arrangement.spacedBy(12.dp)) {
            items(alarms) { alarm ->
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = CardBackground),
                    elevation = CardDefaults.cardElevation(defaultElevation = 2.dp),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Row(
                        modifier = Modifier.padding(16.dp),
                        verticalAlignment = Alignment.Top
                    ) {
                        val iconColor = if (alarm.isCritical) Color.Red else Color(0xFFF7A333)
                        Icon(Icons.Default.Warning, contentDescription = "Alarm", tint = iconColor)
                        Spacer(modifier = Modifier.width(12.dp))
                        Column(modifier = Modifier.weight(1f)) {
                            Text(alarm.binName, fontWeight = FontWeight.Bold, fontSize = 16.sp, color = TextPrimary)
                            Spacer(modifier = Modifier.height(4.dp))
                            Text(alarm.message, fontSize = 14.sp, color = TextSecondary)
                        }
                        Text(alarm.time, fontSize = 12.sp, color = TextSecondary)
                    }
                }
            }
        }
    }
}
