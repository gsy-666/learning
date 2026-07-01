package com.gsy.machinecontrol.ui.widgets

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.android.InternalPlatformTextApi
import androidx.compose.ui.unit.dp
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin

@Composable
fun BinDial(
    selectedBin: Int,
    modifier: Modifier = Modifier,
) {
    val targetDeg = when (selectedBin.coerceIn(0, 2)) {
        0 -> -90f
        1 -> 30f
        else -> 150f
    }

    val needleDeg by animateFloatAsState(targetValue = targetDeg, label = "needle")

    Box(
        modifier = modifier
            .background(Color(0xFF0B0F14), RoundedCornerShape(16.dp))
            .padding(16.dp)
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val w = size.width
            val h = size.height
            val r = min(w, h) * 0.42f
            val center = Offset(w / 2f, h / 2f)

            val ringColor = Color(0xFF30363D)
            val textColor = Color(0xFFE6EDF3)
            val accent = Color(0xFF2F81F7)

            // outer ring
            drawCircle(color = ringColor, radius = r, center = center, style = Stroke(width = 6f))

            // bins: 3 sectors marks
            val angles = floatArrayOf(-90f, 30f, 150f)
            val labels = arrayOf("Bin 0", "Bin 1", "Bin 2")

            for (i in 0..2) {
                val a = angles[i] * (PI.toFloat() / 180f)
                val p1 = Offset(center.x + cos(a) * (r * 0.65f), center.y + sin(a) * (r * 0.65f))
                val p2 = Offset(center.x + cos(a) * (r * 1.05f), center.y + sin(a) * (r * 1.05f))
                drawLine(color = ringColor, start = p1, end = p2, strokeWidth = 6f)

                // label boxes
                val bx = center.x + cos(a) * (r * 1.18f)
                val by = center.y + sin(a) * (r * 1.18f)
                drawCircle(color = if (i == selectedBin) Color(0xFF238636) else Color(0xFF151A22), radius = 34f, center = Offset(bx, by))
            }

            // Needle
            val na = needleDeg * (PI.toFloat() / 180f)
            val needleLen = r * 0.9f
            val tip = Offset(center.x + cos(na) * needleLen, center.y + sin(na) * needleLen)
            drawLine(color = accent, start = center, end = tip, strokeWidth = 10f)

            // arrow head
            val head = Path().apply {
                val back = Offset(center.x + cos(na) * (needleLen * 0.78f), center.y + sin(na) * (needleLen * 0.78f))
                val left = Offset(
                    back.x + cos(na + (PI.toFloat() * 0.75f)) * 22f,
                    back.y + sin(na + (PI.toFloat() * 0.75f)) * 22f,
                )
                val right = Offset(
                    back.x + cos(na - (PI.toFloat() * 0.75f)) * 22f,
                    back.y + sin(na - (PI.toFloat() * 0.75f)) * 22f,
                )
                moveTo(tip.x, tip.y)
                lineTo(left.x, left.y)
                lineTo(right.x, right.y)
                close()
            }
            drawPath(head, color = accent)

            // center hub
            drawCircle(color = Color(0xFFE6EDF3), radius = 14f, center = center)
            drawCircle(color = Color(0xFF0B0F14), radius = 8f, center = center)
        }
    }
}
