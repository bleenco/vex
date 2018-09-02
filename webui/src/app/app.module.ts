import { BrowserModule } from '@angular/platform-browser';
import { NgModule } from '@angular/core';

import { SocketService } from './providers/socket.service';

import { AppComponent } from './app.component';

@NgModule({
  declarations: [
    AppComponent
  ],
  imports: [
    BrowserModule
  ],
  providers: [SocketService],
  bootstrap: [AppComponent]
})
export class AppModule { }
