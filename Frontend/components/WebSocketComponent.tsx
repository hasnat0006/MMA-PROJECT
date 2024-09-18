"use client";
import React, { useEffect, useRef, useState } from "react";

const WebSocketComponent: React.FC = () => {
  const [ws, setWs] = useState<WebSocket | null>(null);
  const [red, setRed] = useState<number>(0);
  const [ir, setIR] = useState<number>(0);
  const [green, setGreen] = useState<number>(0);
  const [temp, setTemp] = useState<number>(0);
  const [stepCount, setStepCount] = useState<number>(0);
  const [beatsPerMinute, setBeatsPerMinute] = useState<number>(0);
  const [beatAvg, setBeatAvg] = useState<number>(0);
  const [uv, setUV] = useState<number>(0);

  const RATE_SIZE = 4; // Number of heart rates to average
  const MINIMUM_DELTA = 300; // Minimum time between two beats (in milliseconds)
  const MAXIMUM_DELTA = 1500; // Maximum time between two beats (in milliseconds)

  // UseRef for mutable variables that don't cause re-renders
  const rates = useRef<number[]>(new Array(RATE_SIZE).fill(0));
  const rateSpot = useRef<number>(0);
  const lastBeat = useRef<number>(0);

  useEffect(() => {
    const websocket = new WebSocket("ws://192.168.7.97/ws");

    websocket.onopen = () => {
      console.log("Connected to WebSocket server");
    };

    websocket.onmessage = (event: MessageEvent) => {
      try {
        const data = JSON.parse(event.data);
        console.log(data);

        if (data.t === "rig") {
          setRed(data.dt.r);
          setIR(data.dt.ir);
          setGreen(data.dt.g);
          processHeartRate(data.dt.ir);
        }

        if (data.t === "tmp") {
          setTemp(data.dt.T);
          setUV(data.dt.U);
        }

        if (data.t === "s") {
          setStepCount(data.dt.s);
        }
      } catch (error) {
        console.error("Error parsing WebSocket message:", event.data, error);
      }
    };

    websocket.onerror = (event: Event) => {
      console.log("WebSocket error", event);
    };

    websocket.onclose = () => {
      console.log("WebSocket connection closed");
    };

    setWs(websocket);

    return () => {
      websocket.close();
    };
  }, []);

  const processHeartRate = (irValue: number) => {
    const currentTime = new Date().getTime();

    if (checkForBeat(irValue) === false) {
      // setBeatsPerMinute(0);
      return;
    }

    // Initialize lastBeat on first valid beat detection
    if (lastBeat.current === 0) {
      lastBeat.current = currentTime;
      return; // Return since we don't have a valid delta yet
    }

    const delta = currentTime - lastBeat.current;
    console.log(delta, MINIMUM_DELTA, MAXIMUM_DELTA); // Log delta and the limits for debugging

    // Only process if delta is within valid range
    if (delta > MINIMUM_DELTA && delta < MAXIMUM_DELTA) {
      lastBeat.current = currentTime;

      const bpm = 60 / (delta / 1000); // Calculate BPM
      if (bpm < 180 && bpm > 40) {
        rates.current[rateSpot.current] = bpm;
        rateSpot.current = (rateSpot.current + 1) % rates.current.length;

        const avgBPM =
          rates.current.reduce((a, b) => a + b, 0) / rates.current.length;
        setBeatsPerMinute(bpm);
        setBeatAvg(avgBPM);
      }
    }
  };

  const checkForBeat = (irValue: number) => {
    return irValue > 50000;
  };

  return (
    <div>
      <h1>WebSocket Heart Rate Monitor</h1>
      <div>
        <p>Red: {red}</p>
        <p>IR: {ir}</p>
        <p>Green: {green}</p>
        <p>Temperature: {temp}°C</p>
        <p>UV Index: {uv}</p>
        <p>Step count: {stepCount}</p>
        <p>Beats per minute: {beatsPerMinute}</p>
        <p>Average BPM: {beatAvg}</p>
      </div>
    </div>
  );
};

export default WebSocketComponent;
