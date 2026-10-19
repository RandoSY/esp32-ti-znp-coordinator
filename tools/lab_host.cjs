'use strict';
// Run the installed, unchanged Zigbee2MQTT module and a local Mosquitto broker.
// A stop file routes shutdown through Zigbee2MQTT's normal SIGINT handler.
const fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process');
const root=path.resolve(__dirname,'..'),data=path.join(root,'lab-zigbee2mqtt');
const state=path.join(data,'lab-process.json'),stop=path.join(data,'stop.request');
const entry=path.join(root,'build/zigbee2mqtt/index.js');
const brokerExe=path.join(process.env.LOCALAPPDATA,'ESP32-ZNP-Lab/mosquitto/mosquitto.exe');
let broker=null,stopping=false;
process.env.ZIGBEE2MQTT_DATA=data;
function listening(port){return new Promise(resolve=>{const s=net.connect({host:'127.0.0.1',port});s.setTimeout(1000);s.once('connect',()=>{s.destroy();resolve(true);});s.once('error',()=>resolve(false));s.once('timeout',()=>{s.destroy();resolve(false);});});}
function stopNormally(){if(stopping)return;stopping=true;process.emit('SIGINT','SIGINT');}
process.on('exit',()=>{if(broker)broker.kill();try{fs.unlinkSync(state);}catch{}});
(async()=>{
    for(const f of [entry,brokerExe,path.join(data,'configuration.yaml'),path.join(data,'mosquitto.conf')])if(!fs.existsSync(f))throw new Error(`Required installation/configuration missing: ${f}`);
    if(fs.existsSync(stop))fs.unlinkSync(stop);
    if(await listening(8080))throw new Error('Port 8080 is already in use; lab may already be running.');
    if(!await listening(1883)){
        broker=spawn(brokerExe,['-c',path.join(data,'mosquitto.conf')],{windowsHide:true,stdio:'inherit'});
        broker.on('error',e=>{console.error(e);process.exitCode=1;stopNormally();});
        broker.on('exit',code=>{if(!stopping){console.error(`Mosquitto exited: ${code}`);stopNormally();}});
        for(let n=0;n<30 && !await listening(1883);++n)await new Promise(r=>setTimeout(r,100));
        if(!await listening(1883))throw new Error('Mosquitto did not open port 1883.');
    }else console.log('Using the MQTT broker already listening on 127.0.0.1:1883.');
    fs.writeFileSync(state,JSON.stringify({pid:process.pid,brokerPid:broker?.pid||null,started:new Date().toISOString()},null,2));
    const app=require(entry);
    setInterval(()=>{if(fs.existsSync(stop)){fs.unlinkSync(stop);stopNormally();}},500).unref();
    await app.start();
    console.log('Lab frontend: http://127.0.0.1:8080; use Stop-Lab.cmd before Python serial tools.');
})().catch(e=>{console.error(e.stack);process.exit(1);});
