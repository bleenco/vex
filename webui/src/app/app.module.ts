import { BrowserModule } from '@angular/platform-browser';
import { NgModule } from '@angular/core';

import { SocketService } from './providers/socket.service';
import { DataService } from './shared/data.service';

import { AppComponent } from './app.component';
import { HeaderComponent } from './components/header/header.component';
import { ClientListComponent } from './components/client-list/client-list.component';
import { ClientListItemComponent } from './components/client-list-item/client-list-item.component';

@NgModule({
  declarations: [
    AppComponent,
    HeaderComponent,
    ClientListComponent,
    ClientListItemComponent
  ],
  imports: [
    BrowserModule
  ],
  providers: [SocketService, DataService],
  bootstrap: [AppComponent]
})
export class AppModule { }
