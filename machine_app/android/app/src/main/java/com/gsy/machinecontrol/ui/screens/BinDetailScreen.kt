package com.gsy.machinecontrol.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import kotlinx.coroutines.delay
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.BaselineShift
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.navigation.NavController
import com.gsy.machinecontrol.ui.theme.*
import com.gsy.machinecontrol.ui.widgets.SimpleLineChart
import com.gsy.machinecontrol.viewmodel.MachineViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun BinDetailScreen(viewModel: MachineViewModel, navController: NavController, binId: Int) {
    val bins by viewModel.bins.collectAsState()
    val lastStatus by viewModel.lastStatus.collectAsState()
    val tempSeries by viewModel.temperatureSeries.collectAsState()
    val humiditySeries by viewModel.humiditySeries.collectAsState()
    val co2Series by viewModel.co2Series.collectAsState()
    val bin = bins.find { it.id == binId } ?: return

    var showPickDialog by remember { mutableStateOf(false) }
    var showPickStatusDialog by remember { mutableStateOf(false) }
    var pickGramsText by remember { mutableStateOf("") }

    var showHealthDialog by remember { mutableStateOf(false) }
    var healthResultText by remember { mutableStateOf("") }

    var showRecognizeDialog by remember { mutableStateOf(false) }
    var recognizeResultText by remember { mutableStateOf("") }

    val pickWeightG by viewModel.pickWeightG.collectAsState()
    val pickResult by viewModel.pickResult.collectAsState()

    LaunchedEffect(showRecognizeDialog) {
        if (showRecognizeDialog) {
            delay(3000)
            viewModel.recognizeBin(binId)
            showRecognizeDialog = false
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(bin.name, fontSize = 18.sp, fontWeight = FontWeight.Bold) },
                navigationIcon = {
                    IconButton(onClick = { navController.popBackStack() }) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = Background)
            )
        }
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(Background)
                .padding(paddingValues)
                .padding(16.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(containerColor = CardBackground),
                shape = RoundedCornerShape(16.dp)
            ) {
                Row(modifier = Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
                    Box(modifier = Modifier.size(50.dp).background(GreenSecondary, RoundedCornerShape(8.dp)))
                    Spacer(modifier = Modifier.width(16.dp))
                    Column {
                        Text("● 正常运行", color = GreenPrimary, fontWeight = FontWeight.Bold)
                        if (bin.healthRiskText != null) {
                            Text("● ${bin.healthRiskText}", color = Color.Red, fontWeight = FontWeight.Bold)
                        }
                        Text(
                            "种类: ${bin.cropType} | 最后更新: ${bin.lastUpdatedText}",
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Medium,
                            color = TextPrimary
                        )
                    }
                }
            }

            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(containerColor = CardBackground),
                shape = RoundedCornerShape(16.dp)
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("环境数据", fontWeight = FontWeight.Bold, fontSize = 16.sp)
                    Spacer(modifier = Modifier.height(16.dp))
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        Column {
                            Text("温度", color = TextSecondary)
                            Text("${bin.temperature} °C", fontSize = 24.sp, fontWeight = FontWeight.Bold)
                        }
                        SimpleLineChart(
                            entries = tempSeries,
                            label = "Temperature",
                            modifier = Modifier
                                .width(180.dp)
                                .height(60.dp)
                        )
                    }
                    HorizontalDivider(modifier = Modifier.padding(vertical = 12.dp))
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        Column {
                            Text("湿度", color = TextSecondary)
                            Text("${lastStatus.humidity} %", fontSize = 24.sp, fontWeight = FontWeight.Bold)
                        }
                        SimpleLineChart(
                            entries = humiditySeries,
                            label = "Humidity",
                            modifier = Modifier
                                .width(180.dp)
                                .height(60.dp)
                        )
                    }
                    HorizontalDivider(modifier = Modifier.padding(vertical = 12.dp))
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        Column {
                            Text(
                                text = buildAnnotatedString {
                                    append("CO")
                                    withStyle(SpanStyle(fontSize = 12.sp, baselineShift = BaselineShift.Subscript)) {
                                        append("2")
                                    }
                                    append(" 浓度")
                                },
                                color = TextSecondary
                            )
                            Text("${bin.co2} ppm", fontSize = 24.sp, fontWeight = FontWeight.Bold)
                        }
                        SimpleLineChart(
                            entries = co2Series,
                            label = "CO2",
                            modifier = Modifier
                                .width(180.dp)
                                .height(60.dp)
                        )
                    }
                }
            }

            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(containerColor = CardBackground),
                shape = RoundedCornerShape(16.dp)
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("通风状态", fontWeight = FontWeight.Bold, fontSize = 16.sp)
                    Spacer(modifier = Modifier.height(8.dp))
                    Row {
                        Text("通风中", fontSize = 18.sp, color = GreenPrimary)
                        Spacer(modifier = Modifier.weight(1f))
                        Text("自动模式", color = TextSecondary)
                    }
                }
            }

            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.Center) {
                Button(
                    onClick = {
                        viewModel.selectBinCommand(binId)
                        showPickDialog = true
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = GreenPrimary)
                ) {
                    Text("选取粮食")
                }
                Spacer(modifier = Modifier.width(12.dp))
                OutlinedButton(
                    onClick = {
                        showRecognizeDialog = true
                        recognizeResultText = "识别中....."
                    }
                ) {
                    Text("识别")
                }
                Spacer(modifier = Modifier.width(12.dp))
                OutlinedButton(
                    onClick = {
                        healthResultText = viewModel.healthCheck(binId)
                        showHealthDialog = true
                    }
                ) {
                    Text("健康检测")
                }
            }
        }

        if (showRecognizeDialog) {
            AlertDialog(
                onDismissRequest = { },
                title = { Text("识别") },
                text = { Text("识别中.....") },
                confirmButton = { }
            )
        }

        if (showHealthDialog) {
            AlertDialog(
                onDismissRequest = { showHealthDialog = false },
                title = { Text("健康检测") },
                text = { Text(healthResultText) },
                confirmButton = {
                    TextButton(onClick = { showHealthDialog = false }) { Text("确定") }
                }
            )
        }

        if (showPickDialog) {
            AlertDialog(
                onDismissRequest = { showPickDialog = false },
                title = { Text("请输入要选取多少克粮食") },
                text = {
                    OutlinedTextField(
                        value = pickGramsText,
                        onValueChange = { pickGramsText = it },
                        singleLine = true,
                        placeholder = { Text("例如：5") },
                        keyboardOptions = androidx.compose.foundation.text.KeyboardOptions(
                            keyboardType = KeyboardType.Number
                        )
                    )
                },
                confirmButton = {
                    TextButton(
                        onClick = {
                            val g = pickGramsText.trim().replace(",", ".").toDoubleOrNull()
                            if (g != null && g > 0) {
                                viewModel.pickGrain(binId, g)
                                showPickDialog = false
                                showPickStatusDialog = true
                            }
                        }
                    ) { Text("确定") }
                },
                dismissButton = {
                    TextButton(onClick = { showPickDialog = false }) { Text("取消") }
                }
            )
        }

        if (showPickStatusDialog) {
            AlertDialog(
                onDismissRequest = { showPickStatusDialog = false },
                title = { Text("选取中") },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("目标: ${pickGramsText} g")
                        Text("实测: ${pickWeightG ?: "--"} g")
                        Text("结果: ${pickResult ?: "执行中..."}")
                    }
                },
                confirmButton = {
                    TextButton(onClick = { showPickStatusDialog = false }) { Text("关闭") }
                }
            )
        }
    }
}
