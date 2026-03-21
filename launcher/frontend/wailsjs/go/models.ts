export namespace detect {
	
	export class Q1Installation {
	    path: string;
	    sourceType: string;
	
	    static createFrom(source: any = {}) {
	        return new Q1Installation(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.path = source["path"];
	        this.sourceType = source["sourceType"];
	    }
	}
	export class Q3Installation {
	    path: string;
	    sourceType: string;
	
	    static createFrom(source: any = {}) {
	        return new Q3Installation(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.path = source["path"];
	        this.sourceType = source["sourceType"];
	    }
	}

}

export namespace engine {
	
	export class ServerConfig {
	    hostname: string;
	    map: string;
	    gameType: string;
	    maxClients: number;
	    password: string;
	    addBots: boolean;
	    botCount: number;
	
	    static createFrom(source: any = {}) {
	        return new ServerConfig(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.hostname = source["hostname"];
	        this.map = source["map"];
	        this.gameType = source["gameType"];
	        this.maxClients = source["maxClients"];
	        this.password = source["password"];
	        this.addBots = source["addBots"];
	        this.botCount = source["botCount"];
	    }
	}

}

export namespace settings {
	
	export class RecentServer {
	    address: string;
	    lastUsed: string;
	
	    static createFrom(source: any = {}) {
	        return new RecentServer(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.address = source["address"];
	        this.lastUsed = source["lastUsed"];
	    }
	}
	export class Settings {
	    playerName: string;
	    renderer: string;
	    customArgs: string;
	    recentServers: RecentServer[];
	
	    static createFrom(source: any = {}) {
	        return new Settings(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.playerName = source["playerName"];
	        this.renderer = source["renderer"];
	        this.customArgs = source["customArgs"];
	        this.recentServers = this.convertValues(source["recentServers"], RecentServer);
	    }
	
		convertValues(a: any, classs: any, asMap: boolean = false): any {
		    if (!a) {
		        return a;
		    }
		    if (a.slice && a.map) {
		        return (a as any[]).map(elem => this.convertValues(elem, classs));
		    } else if ("object" === typeof a) {
		        if (asMap) {
		            for (const key of Object.keys(a)) {
		                a[key] = new classs(a[key]);
		            }
		            return a;
		        }
		        return new classs(a);
		    }
		    return a;
		}
	}

}

