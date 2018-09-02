import { Component, OnInit } from '@angular/core';
import { SocketService } from './providers/socket.service';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html'
})
export class AppComponent implements OnInit {

  constructor(public socket: SocketService) {}

  ngOnInit() {
    this.socket.onMessage().subscribe(event => {
      console.log(event);
    });

    this.socket.emit({ message: 'Serbus!' });
  }
}
