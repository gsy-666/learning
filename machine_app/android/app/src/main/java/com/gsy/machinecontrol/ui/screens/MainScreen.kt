package com.gsy.machinecontrol.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.*
import androidx.navigation.NavHostController
import com.gsy.machinecontrol.ui.theme.GreenPrimary
import com.gsy.machinecontrol.viewmodel.MachineViewModel

@Composable
fun MainScreen(viewModel: MachineViewModel = viewModel()) {
    val navController = rememberNavController()
    var selectedTab by remember { mutableIntStateOf(0) }

    Scaffold(
        bottomBar = {
            NavigationBar(containerColor = Color.White) {
                val items = listOf("首页", "设备", "报警", "我的")
                val icons = listOf(Icons.Default.Home, Icons.Default.Settings, Icons.Default.Notifications, Icons.Default.Person)
                val routes = listOf("home", "device", "alarm", "profile")

                items.forEachIndexed { index, item ->
                    NavigationBarItem(
                        icon = { Icon(icons[index], contentDescription = item) },
                        label = { Text(item) },
                        selected = selectedTab == index,
                        onClick = { 
                            selectedTab = index 
                            navController.navigate(routes[index]) {
                                popUpTo(navController.graph.startDestinationId) { saveState = true }
                                launchSingleTop = true
                                restoreState = true
                            }
                        },
                        colors = NavigationBarItemDefaults.colors(
                            selectedIconColor = GreenPrimary,
                            selectedTextColor = GreenPrimary,
                            unselectedIconColor = Color.LightGray,
                            unselectedTextColor = Color.LightGray,
                            indicatorColor = Color.Transparent
                        )
                    )
                }
            }
        }
    ) { paddingValues ->
        NavHost(
            navController = navController, 
            startDestination = "home", 
            modifier = Modifier.padding(paddingValues)
        ) {
            composable("home") {
                HomeScreen(
                    viewModel = viewModel,
                    onNavigate = { route ->
                        val routes = listOf("home", "device", "alarm", "profile")
                        val tabIndex = routes.indexOf(route)
                        if (tabIndex >= 0) selectedTab = tabIndex

                        navController.navigate(route) {
                            popUpTo(navController.graph.startDestinationId) { saveState = true }
                            launchSingleTop = true
                            restoreState = true
                        }
                    }
                )
            }
            composable("device") { DeviceControlScreen(viewModel) }
            composable("alarm") { AlarmScreen(viewModel) }
            composable("profile") { ProfileScreen(viewModel) }
            composable("overview") { EnvironmentOverviewScreen(viewModel, navController) }
            composable("bin/{binId}") { backStackEntry ->
                val binId = backStackEntry.arguments?.getString("binId")?.toIntOrNull() ?: 0
                BinDetailScreen(viewModel, navController, binId)
            }
        }
    }
}
