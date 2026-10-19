'use strict';
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');
const {ZStackAdapter} = require('../build/host/node_modules/zigbee-herdsman/dist/adapter/z-stack/adapter/zStackAdapter.js');
const options = {panID:0x1A62, extendedPanID:[0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88], channelList:[15], networkKey:Array.from({length:16},(_,i)=>i), networkKeyDistribute:false};
const port=Number(process.argv[2]);
const backupPath=path.resolve(process.argv[3]);
async function session(expected) {
    const adapter=new ZStackAdapter(options,{path:`tcp://127.0.0.1:${port}`,baudRate:115200,rtscts:false},backupPath,{disableLED:false});
    try {
        const result=await adapter.start();
        assert.equal(result,expected);
        const version=await adapter.getCoordinatorVersion();
        assert.equal(version.meta.product,1);
        const network=await adapter.getNetworkParameters();
        assert.equal(network.panID,options.panID);assert.equal(network.channel,15);assert.equal(network.extendedPanID,'0x1122334455667788');
        await adapter.permitJoin(0,0);
        const backup=await adapter.backup([]);
        assert.equal(backup.networkOptions.panId,options.panID);
        assert.deepEqual([...backup.networkOptions.networkKey],options.networkKey);
        assert.equal(backup.networkKeyInfo.frameCounter,0);
        console.log(`PASS: unmodified herdsman ${expected}, identity, network settings, permit-join close, network backup`);
    } finally {await adapter.stop();}
}
const timer=setTimeout(()=>{console.error('Host compatibility check timed out');process.exit(2);},90000);
(async()=>{
    fs.mkdirSync(path.dirname(backupPath),{recursive:true});
    await session('reset');
    await session('resumed');
})().then(()=>clearTimeout(timer),error=>{clearTimeout(timer);console.error(error.stack);process.exitCode=1;});
