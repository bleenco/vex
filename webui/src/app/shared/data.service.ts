import { Injectable } from '@angular/core';
import { Client } from './client.model';
import { SocketService } from '../providers/socket.service';
import { Subscription } from 'rxjs';

@Injectable({
  providedIn: 'root'
})
export class DataService {
  clients: Client[] = [];
  sub: Subscription;

  constructor(public socket: SocketService) { }

  initSocketEvents(): void {
    this.sub = this.socket.onMessage().subscribe(e => {
      if (e.type === 'clients') {
        this.setClients(e.data);
      }
    });
  }

  deinitSocketEvents(): void {
    if (this.sub) {
      this.sub.unsubscribe();
    }
  }

  private setClients(clients: any[]): void {
    if (!clients || !clients.length) {
      return;
    }

    clients.forEach(c => {
      const client = this.clients.find(cl => c.id === cl.id);
      if (client) {
        client.setHosts(c.hosts);
      } else {
        const cl = new Client(c.id);
        cl.setHosts(c.hosts);
        this.clients.push(cl);
      }
    });
  }
}
