package com.gsy.machinecontrol.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val GreenPrimary = Color(0xFF2FA862)
val GreenSecondary = Color(0xFFE8F5EE)
val TextPrimary = Color(0xFF333333)
val TextSecondary = Color(0xFF888888)
val Background = Color(0xFFF7F9F8)
val CardBackground = Color(0xFFFFFFFF)

private val LightColors = lightColorScheme(
    primary = GreenPrimary,
    secondary = GreenSecondary,
    background = Background,
    surface = CardBackground,
    onPrimary = Color.White,
    onSecondary = TextPrimary,
    onBackground = TextPrimary,
    onSurface = TextPrimary,
)

@Composable
fun MachineControlTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = LightColors, // strictly forcing light theme per the mockup
        content = content
    )
}
