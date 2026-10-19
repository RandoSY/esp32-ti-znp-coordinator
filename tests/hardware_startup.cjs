'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const root=path.resolve(__dirname,'..');
const pkg=path.join(root,'build/host/node_modules/zigbee-herdsman/dist');
const {ZStackAdapter}=require(path.join(pkg,'adapter/z-stack/adapter/zStackAdapter.js'));
const {setLogger}=require(path.join(pkg,'utils/logger.js'));
const dir=path.join(root,'artifacts/esp32c6');fs.mkdirSync(dir,{recursive:true});
const log=fs.createWriteStream(path.join(dir,'hardware-herdsman.log'),{flags:'a'});
function message(level,text,ns){log.write(`${new Date().toISOString()} ${level} ${ns} ${typeof text==='function'?text():text}\n`);}
setLogger({debug:(t,n)=>message('debug',t,n),info:(t,n)=>message('info',t,n),warning:(t,n)=>message('warning',t,n),error:(t,n)=>message('error',t,n)});
const config=path.join(dir,'lab-network.json');
if(!fs.existsSync(config))fs.writeFileSync(config,JSON.stringify({panID:0x4000+crypto.randomInt(0x3FFF),extendedPanID:[...crypto.randomBytes(8)],channelList:[20],networkKey:[...crypto.randomBytes(16)],networkKeyDistribute:false},null,2));
const options=JSON.parse(fs.readFileSync(config));
const port=process.argv[2]||'COM17';
const timer=setTimeout(()=>{console.error('Hardware host test timed out; inspect hardware-herdsman.log');process.exit(2);},150000);
async function run(){
    const adapter=new ZStackAdapter(options,{path:port,baudRate:115200,rtscts:false},path.join(dir,'lab-backup.json'),{disableLED:false});
    try{
        const result=await adapter.start();console.log(`PASS hardware Zigbee2MQTT adapter startup: ${result}`);
        const network=await adapter.getNetworkParameters();assert.equal(network.panID,options.panID);assert.equal(network.channel,20);
        console.log(`PASS real coordinator network parameters: PAN 0x${network.panID.toString(16)}, channel ${network.channel}`);
        await adapter.permitJoin(0,0);console.log('PASS coordinator-only joining close');
        const backup=await adapter.backup([]);assert.deepEqual([...backup.networkOptions.networkKey],options.networkKey);
        console.log('PASS host-visible network key matches the actual radio key');
        await adapter.reset('soft');console.log('PASS hardware soft reset and TI RESET_IND');
    }finally{await adapter.stop();}
}
(async()=>{await run();await new Promise(r=>setTimeout(r,1500));await run();})().then(()=>{clearTimeout(timer);log.end();},err=>{clearTimeout(timer);console.error(err.stack);log.end();process.exitCode=1;});
