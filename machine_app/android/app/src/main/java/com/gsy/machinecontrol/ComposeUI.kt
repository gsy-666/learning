package com.gsy.machinecontrol

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

val PrimaryGreen = Color(0xFF4CAF50)
val BgGray = Color(0xFFF5F7F5)
val TextDark = Color(0xFF333333)
val TextGray = Color(0xFF888888)
val StatusNormal = Color(0xFF4CAF50)
val StatusWarning = Color(0xFFFF9800)

@Composable
fun GreenhouseApp() {
    var currentScreen by remember { mutableStateOf("home") }
    var selectedSilo by remember { mutableStateOf(1) }

    Scaffold(
        bottomBar = {
            if (currentScreen == "home") {
                NavigationBar(containerColor = Color.White) {
                    NavigationBarItem(
                        icon = { Text("🏠") },
                        label = { Text("首页") },
                        selected = true,
                        onClick = { currentScreen = "home" }
                    )
                    NavigationBarItem(
                        icon = { Text("⚙️") },
                        label = { Text("设备") },
                        selected = false,
                        onClick = { currentScreen = "overview" }
                    )
                    NavigationBarItem(
                        icon = { Text("🔔") },
                        label = { Text("报警") },
                        selected = false,
                        onClick = { }
                    )
                    NavigationBarItem(
                        icon = { Text("👤") },
                        label = { Text("我的") },
                        selected = false,
                        onClick = { }
                    )
                }
            }
        }
    ) { padding ->
        Box(modifier = Modifier.padding(padding).fillMaxSize().background(BgGray)) {
            when (currentScreen) {
                "home" -> HomeScreen(
                    onNavigateToOverview = { currentScreen = "overview" },
                    onNavigateToDetail = { silo -> 
                        selectedSilo = silo
                        currentScreen = "detail" 
                    }
                )
                "overview" -> OverviewScreen(onBack = { currentScreen = "home" })
                "detail" -> DetailScreen(siloId = selectedSilo, onBack = { currentScreen = "home" })
            }
        }
    }
}

@Composable
fun HomeScreen(onNavigateToOverview: () -> Unit, onNavigateToDetail: (Int) -> Unit) {
    LazyColumn(modifier = Modifier.fillMaxSize()) {
        item {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(200.dp)
                    .background(Color(0xFFE8F5E9))
                    .padding(16.dp)
            ) {
                Column {
                    Spacer(Modifier.height(24.dp))
                    Text("环境监测", fontSize = 24.sp, fontWeight = FontWeight.Bold, color = TextDark)
                    Text("实时监测温度、CO₂和种类状态", color = TextGray, fontSize = 14.sp)
                }
            }
        }
        item {
            Row(
                modifier = Modifier.fillMaxWidth().padding(16.dp),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                Card(
                    modifier = Modifier.weight(1f).clickable { onNavigateToOverview() },
                    colors = CardDefaults.cardColors(containerColor = Color.White)
                ) {
                    Column(Modifier.padding(16.dp)) {
                        Text("📊", fontSize = 24.sp)
                        Spacer(Modifier.height(8.dp))
                        Text("环境总览", fontWeight = FontWeight.Bold)
                        Text("查看整体环境数据", fontSize = 12.sp, color = TextGray)
                    }
                }
                Card(
                    modifier = Modifier.weight(1f).clickable { onNavigateToOverview() },
                    colors = CardDefaults.cardColors(containerColor = Color.White)
                ) {
                    Column(Modifier.padding(16.dp)) {
                        Text("⚙️", fontSize = 24.sp)
                        Spacer(Modifier.height(8.dp))
                        Text("设备控制", fontWeight = FontWeight.Bold)
                        Text("管理通风等设备", fontSize = 12.sp, color = TextGray)
                    }
                }
            }
        }
        item {
            Text("设备状态", fontWeight = FontWeight.Bold, modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp))
        }
        items(3) { index ->
            val siloId = index + 1
            Card(
                modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 6.dp).clickable { onNavigateToDetail(siloId) },
                colors = CardDefaults.cardColors(containerColor = Color.White)
            ) {
                Row(Modifier.padding(16.dp).fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        modifier = Modifier.size(32.dp).clip(RoundedCornerShape(8.dp)).background(PrimaryGreen),
                        contentAlignment = Alignment.Center
                    ) {
                        Text("$siloId", color = Color.White)
                    }
                    Spacer(Modifier.width(16.dp))
                    Column(Modifier.weight(1f)) {
                        Text("${siloId}号粮仓", fontWeight = FontWeight.Bold)
                        Text(if (siloId == 1) "24.5°C | 620 ppm | 蔬菜"
                             else if (siloId == 2) "22.1°C | 580 ppm | 草莓"
                             else "23.0°C | 650 ppm | 番茄", fontSize = 12.sp, color = TextGray)
                        Text("存储天数：0天", fontSize = 12.sp, color = TextGray)
                    }
                    Text(if (siloId == 3) "● 预警" else "● 正常", color = if (siloId == 3) StatusWarning else StatusNormal, fontSize = 14.sp)
                }
            }
        }
    }
}

@Composable
fun OverviewScreen(onBack: () -> Unit) {
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("〈", modifier = Modifier.clickable { onBack() }.padding(8.dp), fontSize = 20.sp)
            Text("环境总览", modifier = Modifier.weight(1f), fontWeight = FontWeight.Bold, textAlign = TextAlign.Center)
            Text("📅", fontSize = 20.sp)
        }
        LazyColumn(Modifier.padding(horizontal = 16.dp)) {
            item {
                Card(colors = CardDefaults.cardColors(containerColor = Color.White), modifier = Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(16.dp)) {
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                Text("🌡️ 平均温度", color = TextGray, fontSize = 12.sp)
                                Text("23.2°C", fontWeight = FontWeight.Bold, fontSize = 20.sp)
                            }
                            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                Text("☁️ 平均CO₂浓度", color = TextGray, fontSize = 12.sp)
                                Text("617 ppm", fontWeight = FontWeight.Bold, fontSize = 20.sp)
                            }
                            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                Text("💨 通风状态", color = TextGray, fontSize = 12.sp)
                                Text("良好", fontWeight = FontWeight.Bold, fontSize = 20.sp, color = PrimaryGreen)
                            }
                        }
                    }
                }
            }
            item { Spacer(Modifier.height(16.dp)) }
            item {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
                    Text("报警", fontWeight = FontWeight.Bold)
                    Text("全部已读", fontSize = 12.sp, color = TextGray)
                }
                Card(Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(containerColor = Color.White)) {
                    Column(Modifier.padding(16.dp)) {
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                            Text("⚠️ 2号粮仓", color = Color.Red, fontWeight = FontWeight.Bold)
                            Text("09:32", fontSize = 12.sp, color = TextGray)
                        }
                        Text("CO₂浓度偏高 (850ppm)", fontSize = 14.sp)
                        Spacer(Modifier.height(16.dp))
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                            Text("❗ 3号粮仓", color = StatusWarning, fontWeight = FontWeight.Bold)
                            Text("09:18", fontSize = 12.sp, color = TextGray)
                        }
                        Text("温度接近上限 (29.5°C)", fontSize = 14.sp)
                    }
                }
            }
            item { Spacer(Modifier.height(16.dp)) }
            item {
                Text("设备控制", fontWeight = FontWeight.Bold, modifier = Modifier.padding(vertical = 8.dp))
                Card(Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(containerColor = Color.White)) {
                    Column(Modifier.padding(16.dp)) {
                        DeviceControlRow("通风设备", true, "自动模式")
                        DeviceControlRow("遮阳系统", false, "关闭")
                        DeviceControlRow("加热设备", true, "自动模式")
                    }
                }
            }
        }
    }
}

@Composable
fun DeviceControlRow(name: String, checked: Boolean, desc: String) {
    Row(Modifier.fillMaxWidth().padding(vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(name, fontWeight = FontWeight.Bold)
            Text(desc, fontSize = 12.sp, color = TextGray)
        }
        Switch(checked = checked, onCheckedChange = {})
    }
}

@Composable
fun DetailScreen(siloId: Int, onBack: () -> Unit) {
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("〈", modifier = Modifier.clickable { onBack() }.padding(8.dp), fontSize = 20.sp)
            Text("${siloId}号粮仓", modifier = Modifier.weight(1f), fontWeight = FontWeight.Bold, textAlign = TextAlign.Center)
            Text("···", fontSize = 20.sp)
        }
        LazyColumn(Modifier.padding(horizontal = 16.dp)) {
            item {
                Card(Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(containerColor = Color.White)) {
                    Row(Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
                        Box(Modifier.size(48.dp).background(Color(0xFFE8F5E9), RoundedCornerShape(8.dp)))
                        Spacer(Modifier.width(16.dp))
                        Column(Modifier.weight(1f)) {
                            Text("● 正常运行", color = PrimaryGreen, fontWeight = FontWeight.Bold)
                            Text("种类: 蔬菜 | 最后更新: 09:41", fontSize = 12.sp, color = TextGray)
                        }
                    }
                }
            }
            item { Spacer(Modifier.height(16.dp)) }
            item {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text("环境数据", fontWeight = FontWeight.Bold)
                    Text("09:41 更新", fontSize = 12.sp, color = TextGray)
                }
                Card(Modifier.fillMaxWidth().padding(vertical = 8.dp), colors = CardDefaults.cardColors(containerColor = Color.White)) {
                    Column(Modifier.padding(16.dp)) {
                        DataRow("🌡️ 温度", "24.5 °C")
                        Spacer(Modifier.height(16.dp))
                        DataRow("☁️ CO₂ 浓度", "620 ppm")
                        Spacer(Modifier.height(16.dp))
                        DataRow("🌱 种类/作物", "蔬菜")
                    }
                }
            }
            item { Spacer(Modifier.height(16.dp)) }
            item {
                Text("通风状态", fontWeight = FontWeight.Bold)
                Card(Modifier.fillMaxWidth().padding(vertical = 8.dp), colors = CardDefaults.cardColors(containerColor = Color.White)) {
                    Row(Modifier.fillMaxWidth().padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("💨", fontSize = 24.sp)
                        Spacer(Modifier.width(16.dp))
                        Column(Modifier.weight(1f)) {
                            Text("通风中", fontWeight = FontWeight.Bold)
                            Text("自动模式", fontSize = 12.sp, color = TextGray)
                        }
                        Text(">", color = TextGray)
                    }
                }
            }
            item { Spacer(Modifier.height(16.dp)) }
            item {
                Text("历史数据", fontWeight = FontWeight.Bold)
                Card(Modifier.fillMaxWidth().padding(vertical = 8.dp), colors = CardDefaults.cardColors(containerColor = Color.White)) {
                    Box(Modifier.fillMaxWidth().height(150.dp).padding(16.dp), contentAlignment = Alignment.Center) {
                        Text("【图表占位：温度、CO₂曲线】", color = TextGray)
                    }
                }
            }
            item { Spacer(Modifier.height(32.dp)) }
        }
    }
}

@Composable
fun DataRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
        Text(label, color = TextDark, fontWeight = FontWeight.SemiBold)
        Text(value, fontWeight = FontWeight.Bold, fontSize = 20.sp)
    }
}
