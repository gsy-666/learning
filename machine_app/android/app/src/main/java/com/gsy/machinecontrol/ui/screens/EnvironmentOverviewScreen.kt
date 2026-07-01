package com.gsy.machinecontrol.ui.screens

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.BaselineShift
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.navigation.NavController
import com.gsy.machinecontrol.R
import com.gsy.machinecontrol.ui.theme.Background
import com.gsy.machinecontrol.ui.theme.CardBackground
import com.gsy.machinecontrol.ui.theme.GreenPrimary
import com.gsy.machinecontrol.ui.theme.TextPrimary
import com.gsy.machinecontrol.ui.widgets.SimpleLineChart
import com.gsy.machinecontrol.viewmodel.MachineViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EnvironmentOverviewScreen(viewModel: MachineViewModel, navController: NavController) {
    val tempSeries by viewModel.temperatureSeries.collectAsState()
    val co2Series by viewModel.co2Series.collectAsState()

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("环境总览", fontSize = 18.sp, fontWeight = FontWeight.Bold) },
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
        ) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(containerColor = CardBackground),
                shape = RoundedCornerShape(16.dp),
                elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("温度曲线", fontSize = 14.sp, fontWeight = FontWeight.Bold, color = TextPrimary)
                    Spacer(modifier = Modifier.height(8.dp))
                    SimpleLineChart(
                        entries = tempSeries,
                        label = "Temperature",
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(180.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(containerColor = CardBackground),
                shape = RoundedCornerShape(16.dp),
                elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text(
                        buildAnnotatedString {
                            append("CO")
                            withStyle(SpanStyle(fontSize = 12.sp, baselineShift = BaselineShift.Subscript)) {
                                append("2")
                            }
                            append(" 浓度曲线")
                        },
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                        color = TextPrimary
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    SimpleLineChart(
                        entries = co2Series,
                        label = "CO2",
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(180.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                ActionCard(title = "平均温度", subtitle = "--", modifier = Modifier.weight(1f))
                ActionCard(title = "平均CO2", subtitle = "--", modifier = Modifier.weight(1f))
                ActionCard(title = "通风状态", subtitle = "--", modifier = Modifier.weight(1f))
            }
        }
    }
}
