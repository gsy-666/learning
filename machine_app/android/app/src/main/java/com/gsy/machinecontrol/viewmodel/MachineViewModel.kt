package com.gsy.machinecontrol.viewmodel

import androidx.compose.runtime.mutableStateListOf
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.gsy.machinecontrol.net.GatewayClient
import com.github.mikephil.charting.data.Entry
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

import kotlinx.coroutines.currentCoroutineContext
import java.time.LocalTime
import java.time.format.DateTimeFormatter

private val HHMM: DateTimeFormatter = DateTimeFormatter.ofPattern("HH:mm")
private fun nowHHmm(): String = LocalTime.now().format(HHMM)

data class BinState(
    val id: Int,
    val name: String,
    val temperature: Double,
    val co2: Int,
    val cropType: String,
    val lastUpdatedText: String,
    val status: String,
    val isWarning: Boolean,
    val healthRiskText: String? = null
)

data class AlarmEvent(
    val binName: String,
    val time: String,
    val message: String,
    val isCritical: Boolean
)

data class StatusData(
    val temperature: Double = 0.0,
    val humidity: Double = 0.0,
    val co2: Int = 0
)

private fun parseStatusJson(line: String?): StatusData? {
    if (line.isNullOrBlank()) return null
    if (!line.startsWith("{")) return null

    fun getNumber(key: String): Double? {
        val idx = line.indexOf("\"$key\"")
        if (idx < 0) return null
        val colon = line.indexOf(':', idx)
        if (colon < 0) return null
        var end = colon + 1
        while (end < line.length && line[end] == ' ') end++
        var i = end
        while (i < line.length && (line[i].isDigit() || line[i] == '-' || line[i] == '.' )) i++
        return line.substring(end, i).toDoubleOrNull()
    }

    val t = getNumber("temperature") ?: return null
    val h = getNumber("humidity") ?: 0.0
    val c = getNumber("co2")?.toInt() ?: return null

    if (t < -0.5 || c < 0) return null
    return StatusData(temperature = t, humidity = h, co2 = c)
}

class MachineViewModel : ViewModel() {
    private val client = GatewayClient()

    private val _isConnected = MutableStateFlow(false)
    val isConnected: StateFlow<Boolean> = _isConnected

    private val _debugLastRawStatusLine = MutableStateFlow("")
    val debugLastRawStatusLine: StateFlow<String> = _debugLastRawStatusLine

    private val _lastStatus = MutableStateFlow(StatusData())
    val lastStatus: StateFlow<StatusData> = _lastStatus

    private val _debugLogLine = MutableStateFlow("")
    val debugLogLine: StateFlow<String> = _debugLogLine

    private val _temperatureSeries = MutableStateFlow<List<Entry>>(emptyList())
    val temperatureSeries: StateFlow<List<Entry>> = _temperatureSeries

    private val _co2Series = MutableStateFlow<List<Entry>>(emptyList())
    val co2Series: StateFlow<List<Entry>> = _co2Series

    private val _humiditySeries = MutableStateFlow<List<Entry>>(emptyList())
    val humiditySeries: StateFlow<List<Entry>> = _humiditySeries

    private var seriesX = 0f

    private var statusJob: Job? = null

    private val _pickWeightG = MutableStateFlow<Int?>(null)
    val pickWeightG: StateFlow<Int?> = _pickWeightG

    private val _pickResult = MutableStateFlow<String?>(null)
    val pickResult: StateFlow<String?> = _pickResult

    private var pickPollJob: Job? = null

    // Mock data for the UI
    private val _bins = MutableStateFlow(
        listOf(
            BinState(0, "1号粮仓", 24.5, 620, "蔬菜", "--:--", "正常", false),
            BinState(1, "2号粮仓", 22.1, 580, "草莓", "--:--", "正常", false),
            BinState(2, "3号粮仓", 23.0, 650, "番茄", "--:--", "预警", true)
        )
    )
    val bins: StateFlow<List<BinState>> = _bins

    private val _alarms = MutableStateFlow(
        listOf(
            AlarmEvent("2号粮仓", "09:32", "CO2 浓度偏高 (850ppm)", true),
            AlarmEvent("3号粮仓", "09:18", "温度接近上限 (29.5°C)", false)
        )
    )
    val alarms: StateFlow<List<AlarmEvent>> = _alarms

    private val _automationMode = MutableStateFlow(true)
    val automationMode: StateFlow<Boolean> = _automationMode

    private val _ventilationState = MutableStateFlow(true)
    val ventilationState: StateFlow<Boolean> = _ventilationState

    init {
        // no auto-connect; connect from Profile screen
    }

    fun connect(host: String, port: Int) {
        statusJob?.cancel()
        client.disconnect({}, {})
        _isConnected.value = false

        val log: (String) -> Unit = { msg ->
            _debugLogLine.value = msg
        }

        client.connect(
            host = host,
            port = port,
            onLog = log,
            onDisconnected = { reason ->
                log("Disconnected cb: $reason")
                _isConnected.value = false
                statusJob?.cancel()
            },
            onConnected = {
                log("onConnected")
                _isConnected.value = true

                statusJob = viewModelScope.launch {
                    while (currentCoroutineContext().isActive && client.isConnected()) {
                        client.requestLine(
                            "STATUS",
                            timeoutMs = 1500,
                            onLog = log,
                            onDisconnected = { r -> log("req disconnected: $r") },
                            onResult = { line ->
                                if (line != null) _debugLastRawStatusLine.value = line
                                val parsed = parseStatusJson(line)
                                if (parsed != null) {
                                    _lastStatus.value = parsed
                                    _bins.value = _bins.value.map { it.copy(temperature = parsed.temperature, co2 = parsed.co2) }

                                    seriesX += 1f
                                    val maxPoints = 120

                                    _temperatureSeries.value = (_temperatureSeries.value + Entry(seriesX, parsed.temperature.toFloat()))
                                        .takeLast(maxPoints)

                                    _humiditySeries.value = (_humiditySeries.value + Entry(seriesX, parsed.humidity.toFloat()))
                                        .takeLast(maxPoints)

                                    _co2Series.value = (_co2Series.value + Entry(seriesX, parsed.co2.toFloat()))
                                        .takeLast(maxPoints)
                                }
                            }
                        )
                        delay(1000)
                    }
                    log("statusJob end")
                }
            }
        )
    }

    fun disconnect() {
        statusJob?.cancel()
        client.disconnect({}, {})
        _isConnected.value = false
        _temperatureSeries.value = emptyList()
        _humiditySeries.value = emptyList()
        _co2Series.value = emptyList()
        seriesX = 0f
    }

    fun toggleVentilation(enabled: Boolean) {
        _ventilationState.value = enabled
        if (enabled) {
            client.send("ENABLE", {}, {})
        } else {
            client.send("DISABLE", {}, {})
        }
    }

    fun toggleAutomation(enabled: Boolean) {
        _automationMode.value = enabled
    }

    fun selectBinCommand(binId: Int) {
        client.send("BIN $binId", {}, {})
    }

    fun pickGrain(binId: Int, grams: Double) {
        _pickResult.value = null
        _pickWeightG.value = null

        pickPollJob?.cancel()
        pickPollJob = viewModelScope.launch {
            while (currentCoroutineContext().isActive && client.isConnected()) {
                client.requestLine(
                    "WEIGHT",
                    timeoutMs = 800,
                    onLog = { _debugLogLine.value = it },
                    onDisconnected = { _debugLogLine.value = "req disconnected: $it" },
                    onResult = { line ->
                        val w = line?.substringAfter("\"weight_g\":")?.substringBefore('}')?.toIntOrNull()
                        if (w != null) _pickWeightG.value = w
                    }
                )
                delay(500)
            }
        }

        client.requestLine(
            "PICK $binId $grams",
            timeoutMs = 70000,
            onLog = { _debugLogLine.value = it },
            onDisconnected = { _debugLogLine.value = "req disconnected: $it" },
            onResult = { line ->
                val ok = line?.contains("\"ok\":1") == true
                val err = line?.substringAfter("\"err\":\"")?.substringBefore('"')
                _pickResult.value = if (ok) "OK" else (err ?: (line ?: "ERR"))
                pickPollJob?.cancel()
            }
        )
    }

    fun recognizeBin(binId: Int) {
        val crop = when (binId) {
            0 -> "大米"
            1 -> "玉米"
            2 -> "小米"
            else -> "未知"
        }
        val now = nowHHmm()
        _bins.value = _bins.value.map { b ->
            if (b.id == binId) b.copy(cropType = crop, lastUpdatedText = now) else b
        }
    }

    fun healthCheck(binId: Int): String {
        val (result, status, isWarning, riskText) = when (binId) {
            0, 1 -> Quad("健康", "正常", false, null)
            2 -> Quad("受潮风险", "预警", true, "受潮风险")
            else -> Quad("未知", "正常", false, null)
        }

        val now = nowHHmm()
        var matchedBin: BinState? = null
        _bins.value = _bins.value.map { b ->
            if (b.id == binId) {
                matchedBin = b
                b.copy(
                    status = status,
                    isWarning = isWarning,
                    lastUpdatedText = now,
                    healthRiskText = riskText
                )
            } else {
                b
            }
        }

        val b = matchedBin
        if (b != null && result == "受潮风险") {
            _alarms.value = listOf(
                AlarmEvent(b.name, now, "受潮风险", true)
            ) + _alarms.value
        }

        return result
    }

    private data class Quad(
        val result: String,
        val status: String,
        val isWarning: Boolean,
        val riskText: String?
    )
}
