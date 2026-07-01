package com.gsy.machinecontrol.ui.screens

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.gsy.machinecontrol.R
import com.gsy.machinecontrol.ui.theme.*
import com.gsy.machinecontrol.viewmodel.MachineViewModel

@Composable
fun HomeScreen(viewModel: MachineViewModel, onNavigate: (String) -> Unit) {
    val bins by viewModel.bins.collectAsState()

    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .background(Background)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        item { HeaderSection() }
        item { GraphicSection() }
        item {
            ActionButtonsSection(
                onOverviewClick = { onNavigate("overview") },
                onDeviceClick = { onNavigate("device") }
            )
        }
        item {
            Text("设备状态", fontSize = 18.sp, fontWeight = FontWeight.Bold, color = TextPrimary)
        }
        items(bins) { bin ->
            DeviceStatusItem(
                title = bin.name,
                details = "${bin.temperature}°C | ${bin.co2} ppm | ${bin.cropType}",
                status = bin.status,
                statusColor = if (bin.isWarning) Color(0xFFF7A333) else GreenPrimary,
                onClick = { onNavigate("bin/${bin.id}") }
            )
        }
    }
}

@Composable
fun HeaderSection() {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column {
            Text("环境监测", fontSize = 24.sp, fontWeight = FontWeight.Bold, color = TextPrimary)
            Text("实时监测温度、CO2和种类状态", fontSize = 14.sp, color = TextSecondary)
        }
        IconButton(onClick = { /* TODO */ }) {
            Icon(Icons.Default.Notifications, contentDescription = "通知", tint = TextPrimary)
        }
    }
}

@Composable
fun GraphicSection() {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(180.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(GreenSecondary)
    ) {
        Image(
            painter = painterResource(id = R.drawable.home),
            contentDescription = "home",
            modifier = Modifier.fillMaxSize(),
            contentScale = ContentScale.Crop
        )
    }
}

@Composable
fun ActionButtonsSection(onOverviewClick: () -> Unit, onDeviceClick: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        ActionCard(
            title = "环境总览",
            subtitle = "查看整体环境数据",
            modifier = Modifier
                .weight(1f)
                .clickable { onOverviewClick() }
        )
        ActionCard(
            title = "设备控制",
            subtitle = "管理通风等设备",
            modifier = Modifier
                .weight(1f)
                .clickable { onDeviceClick() }
        )
    }
}

@Composable
fun ActionCard(title: String, subtitle: String, modifier: Modifier = Modifier) {
    Card(
        modifier = modifier,
        colors = CardDefaults.cardColors(containerColor = CardBackground),
        elevation = CardDefaults.cardElevation(defaultElevation = 2.dp),
        shape = RoundedCornerShape(16.dp)
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(title, fontWeight = FontWeight.Bold, fontSize = 16.sp, color = TextPrimary)
            Spacer(modifier = Modifier.height(4.dp))
            Text(subtitle, fontSize = 12.sp, color = TextSecondary)
        }
    }
}

@Composable
fun DeviceStatusItem(title: String, details: String, status: String, statusColor: Color, onClick: () -> Unit) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() },
        colors = CardDefaults.cardColors(containerColor = CardBackground),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp),
        shape = RoundedCornerShape(16.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .background(GreenSecondary),
                contentAlignment = Alignment.Center
            ) {
                Text(title.first().toString(), color = GreenPrimary, fontWeight = FontWeight.Bold)
            }
            Spacer(modifier = Modifier.width(16.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(title, fontWeight = FontWeight.Bold, fontSize = 16.sp, color = TextPrimary)
                Spacer(modifier = Modifier.height(2.dp))
                Text(details, fontSize = 12.sp, color = TextSecondary)
                Text("存储天数：0天", fontSize = 12.sp, color = TextSecondary)
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(modifier = Modifier.size(8.dp).clip(CircleShape).background(statusColor))
                Spacer(modifier = Modifier.width(4.dp))
                Text(status, color = statusColor, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, contentDescription = null, tint = Color.LightGray)
            }
        }
    }
}
