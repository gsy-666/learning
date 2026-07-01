package com.gsy.machinecontrol.net

import java.io.BufferedReader
import java.io.BufferedWriter
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.InetSocketAddress
import java.net.Socket
import java.util.concurrent.Executors
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

class GatewayClient {
    private val ioExecutor = Executors.newSingleThreadExecutor()
    private val readerExecutor = Executors.newSingleThreadExecutor()

    @Volatile private var socket: Socket? = null
    @Volatile private var writer: BufferedWriter? = null
    @Volatile private var reader: BufferedReader? = null

    private val connected = AtomicBoolean(false)
    private val inboundLines = LinkedBlockingQueue<String>()

    fun isConnected(): Boolean = connected.get()

    fun connect(
        host: String,
        port: Int,
        onLog: (String) -> Unit,
        onDisconnected: (String) -> Unit,
        onConnected: () -> Unit
    ) {
        if (!connected.compareAndSet(false, true)) {
            onLog("Already connected")
            return
        }

        inboundLines.clear()

        onLog("Connecting to $host:$port")
        ioExecutor.execute {
            try {
                val s = Socket()
                s.tcpNoDelay = true
                s.connect(InetSocketAddress(host, port), 2500)

                val w = BufferedWriter(OutputStreamWriter(s.getOutputStream(), Charsets.UTF_8))
                val r = BufferedReader(InputStreamReader(s.getInputStream(), Charsets.UTF_8))

                socket = s
                writer = w
                reader = r

                // The gateway writes a banner line when connected; discard it so requestLine() sees payload.
                try {
                    val banner = r.readLine()
                    if (banner != null) onLog("<- $banner")
                } catch (_: Throwable) {
                }

                onLog("Connected")
                onConnected()

                readerExecutor.execute {
                    try {
                        while (connected.get()) {
                            val line = r.readLine() ?: break
                            inboundLines.offer(line)
                            onLog("<- $line")
                        }
                    } catch (t: Throwable) {
                        onLog("Read error: ${t.message}")
                    } finally {
                        disconnectInternal("read_end", onLog, onDisconnected)
                    }
                }
            } catch (t: Throwable) {
                disconnectInternal("connect_failed: ${t.message}", onLog, onDisconnected)
            }
        }
    }

    fun send(line: String, onLog: (String) -> Unit, onDisconnected: (String) -> Unit) {
        if (!connected.get()) {
            onLog("Not connected")
            return
        }

        ioExecutor.execute {
            val w = writer
            if (w == null) {
                disconnectInternal("writer_null", onLog, onDisconnected)
                return@execute
            }

            try {
                w.write(line)
                w.write("\n")
                w.flush()
                onLog("-> $line")
            } catch (t: Throwable) {
                disconnectInternal("send_failed: ${t.message}", onLog, onDisconnected)
            }
        }
    }

    fun requestLine(
        line: String,
        timeoutMs: Long = 1500,
        onLog: (String) -> Unit,
        onDisconnected: (String) -> Unit,
        onResult: (String?) -> Unit
    ) {
        if (!connected.get()) {
            onLog("Not connected")
            onResult(null)
            return
        }

        ioExecutor.execute {
            // Write command directly to avoid deadlocking on the single-thread executor.
            val w = writer
            if (w == null) {
                disconnectInternal("writer_null", onLog, onDisconnected)
                onResult(null)
                return@execute
            }

            try {
                w.write(line)
                w.write("\n")
                w.flush()
                onLog("-> $line")
            } catch (t: Throwable) {
                disconnectInternal("send_failed: ${t.message}", onLog, onDisconnected)
                onResult(null)
                return@execute
            }

            try {
                val deadline = System.currentTimeMillis() + timeoutMs
                var resp: String? = null
                while (System.currentTimeMillis() < deadline) {
                    val waitMs = deadline - System.currentTimeMillis()
                    val got = inboundLines.poll(waitMs.coerceAtMost(250), TimeUnit.MILLISECONDS)
                    if (got == null) continue

                    if (!got.startsWith("{")) continue
                    resp = got
                    break
                }
                onResult(resp)
            } catch (_: InterruptedException) {
                onResult(null)
            }
        }
    }

    fun disconnect(onLog: (String) -> Unit, onDisconnected: (String) -> Unit) {
        disconnectInternal("user", onLog, onDisconnected)
    }

    private fun disconnectInternal(reason: String, onLog: (String) -> Unit, onDisconnected: (String) -> Unit) {
        if (!connected.getAndSet(false)) {
            return
        }

        try { socket?.close() } catch (_: Throwable) {}
        socket = null
        writer = null
        reader = null
        inboundLines.clear()

        onLog("Disconnected ($reason)")
        onDisconnected(reason)
    }
}
