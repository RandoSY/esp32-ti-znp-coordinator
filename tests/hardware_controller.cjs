'use strict';
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'..'),pkg=path.join(root,'build/host/node_modules/zigbee-herdsman/dist');
const {Controller}=require(path.join(pkg,'controller/controller.js'));
const {setLogger}=require(path.join(pkg,'utils/logger.js'));
const dir=path.join(root,'artifacts/esp32c6/controller-test');fs.mkdirSync(dir,{recursive:true});
const log=fs.createWriteStream(path.join(dir,'controller.log'),{flags:'a'});
function write(level,t,n){log.write(`${new Date().toISOString()} ${level} ${n} ${typeof t==='function'?t():t}\n`);}
setLogger({debug:(t,n)=>write('debug',t,n),info:(t,n)=>write('info',t,n),warning:(t,n)=>write('warning',t,n),error:(t,n)=>write('error',t,n)});
const options=JSON.parse(fs.readFileSync(path.join(root,'artifacts/esp32c6/lab-network.json')));
const controller=new Controller({network:options,transport:{path:process.argv[2]||'COM17',adapter:'zstack',baudRate:115200,rtscts:false},databasePath:path.join(dir,'database.db'),databaseBackupPath:path.join(dir,'database.db.backup'),backupPath:path.join(dir,'coordinator_backup.json'),adapter:{disableLED:false},acceptJoiningDeviceHandler:async()=>true});
controller.on('deviceJoined',e=>console.log('DEVICE JOINED:',e.device.ieeeAddr));
controller.on('deviceInterview',e=>console.log('DEVICE INTERVIEW:',e.status,e.device.ieeeAddr));
controller.on('message',e=>console.log('DEVICE MESSAGE:',e.type,e.cluster,e.device.ieeeAddr,JSON.stringify(e.data)));
const monitorSeconds=Math.max(0,Number(process.argv[4]||0));
const timer=setTimeout(()=>{console.error('Controller test timed out; see controller.log');process.exit(2);},120000+(Math.max(0,Number(process.argv[3]||0))+monitorSeconds)*1000);
(async()=>{
    try {
        const result=await controller.start();console.log('PASS: full unmodified herdsman Controller.start:',result);
        const coordinator=controller.getDevices().find(d=>d.type==='Coordinator');
        console.log('PASS: coordinator descriptors loaded for endpoints',coordinator.endpoints.map(e=>e.ID).join(','));
        for(const device of controller.getDevices())if(device.type!=='Coordinator' && device.interviewState!=='SUCCESSFUL'){console.log('Retrying saved device interview:',device.ieeeAddr);await device.interview(true);}
        const pairSeconds=Number(process.argv[3]||0);
        await controller.permitJoin(pairSeconds);
        if(pairSeconds){console.log(`JOINING OPEN for ${pairSeconds}s: put your test device into pairing mode now`);await new Promise(r=>setTimeout(r,pairSeconds*1000));}
        await controller.permitJoin(0);
        if(monitorSeconds){console.log(`MONITORING saved devices for ${monitorSeconds}s; joining closed. Operate the contact sensor now.`);await new Promise(r=>setTimeout(r,monitorSeconds*1000));}
    }finally{await controller.stop();}
})().then(()=>{clearTimeout(timer);log.end();},e=>{clearTimeout(timer);console.error(e.stack);log.end();process.exitCode=1;});
